#include "vrviewer.hpp"

#include "openxrmanagerimpl.hpp"
#include "vrenvironment.hpp"
#include "vrsession.hpp"
#include "vrframebuffer.hpp"
#include "vrview.hpp"

#include "../mwrender/vismask.hpp"

#include <osgViewer/Renderer>

#include <components/sceneutil/mwshadowtechnique.hpp>

#include <components/misc/stringops.hpp>

namespace MWVR
{

    const std::array<const char*, 2> VRViewer::sViewNames = {
            "LeftEye",
            "RightEye"
    };

    // Callback to do construction with a graphics context
    class RealizeOperation : public osg::GraphicsOperation
    {
    public:
        RealizeOperation() : osg::GraphicsOperation("VRRealizeOperation", false) {};
        void operator()(osg::GraphicsContext* gc) override;
        bool realized();

    private:
    };

    VRViewer::VRViewer(
        osg::ref_ptr<osgViewer::Viewer> viewer)
        : mViewer(viewer)
        , mPreDraw(new PredrawCallback(this))
        , mPostDraw(new PostdrawCallback(this))
        , mVrShadow()
        , mConfigured(false)
    {
        mViewer->setRealizeOperation(new RealizeOperation());
    }

    VRViewer::~VRViewer(void)
    {
    }

    void VRViewer::traversals()
    {
        mViewer->updateTraversal();
        mViewer->renderingTraversals();
    }

    int parseResolution(std::string conf, int recommended, int max)
    {
        if (Misc::StringUtils::isNumber(conf))
        {
            int res = std::atoi(conf.c_str());
            if (res <= 0)
                return recommended;
            if (res > max)
                return max;
            return res;
        }
        conf = Misc::StringUtils::lowerCase(conf);
        if (conf == "auto" || conf == "recommended")
        {
            return recommended;
        }
        if (conf == "max")
        {
            return max;
        }
    }

    void VRViewer::realize(osg::GraphicsContext* context)
    {
        std::unique_lock<std::mutex> lock(mMutex);

        if (mConfigured)
        {
            return;
        }

        // Give the main camera an initial draw callback that disables camera setup (we don't want it)
        auto mainCamera = mCameras["MainCamera"] = mViewer->getCamera();
        mainCamera->setName("Main");
        mainCamera->setInitialDrawCallback(new VRView::InitialDrawCallback());

        auto* xr = Environment::get().getManager();
        xr->realize(context);

        // Run through initial events to start session
        // For the rest of runtime this is handled by vrsession
        xr->handleEvents();

        // Small feature culling
        bool smallFeatureCulling = Settings::Manager::getBool("small feature culling", "Camera");
        auto smallFeatureCullingPixelSize = Settings::Manager::getFloat("small feature culling pixel size", "Camera");
        osg::Camera::CullingMode cullingMode = osg::Camera::DEFAULT_CULLING | osg::Camera::FAR_PLANE_CULLING;
        if (!smallFeatureCulling)
            cullingMode &= ~osg::CullStack::SMALL_FEATURE_CULLING;
        else
            cullingMode |= osg::CullStack::SMALL_FEATURE_CULLING;

        // Configure eyes, their cameras, and their enslavement.
        osg::Vec4 clearColor = mainCamera->getClearColor();
        auto config = xr->getRecommendedSwapchainConfig();
        bool mirror = Settings::Manager::getBool("mirror texture", "VR");
        std::string mirrorTextureEyeString = Settings::Manager::getString("mirror texture eye", "VR");
        mirrorTextureEyeString = Misc::StringUtils::lowerCase(mirrorTextureEyeString);
        if (mirrorTextureEyeString == "left" || mirrorTextureEyeString == "both")
            mMirrorTextureViews.push_back(sViewNames[(int)Side::LEFT_SIDE]);
        if (mirrorTextureEyeString == "right" || mirrorTextureEyeString == "both")
            mMirrorTextureViews.push_back(sViewNames[(int)Side::RIGHT_SIDE]);
        if (Settings::Manager::getBool("flip mirror texture order", "VR"))
            std::reverse(mMirrorTextureViews.begin(), mMirrorTextureViews.end());
        // TODO: If mirror is false either hide the window or paste something meaningful into it.
        // E.g. Fanart of Dagoth UR wearing a VR headset

        std::array<std::string, 2> xConfString;
        std::array<std::string, 2> yConfString;
        xConfString[0] = Settings::Manager::getString("left eye resolution x", "VR");
        yConfString[0] = Settings::Manager::getString("left eye resolution y", "VR");

        xConfString[1] = Settings::Manager::getString("right eye resolution x", "VR");
        yConfString[1] = Settings::Manager::getString("right eye resolution y", "VR");

        for (unsigned i = 0; i < sViewNames.size(); i++)
        {
            auto name = sViewNames[i];

            config[i].selectedWidth = parseResolution(xConfString[i], config[i].recommendedWidth, config[i].maxWidth);
            config[i].selectedHeight = parseResolution(yConfString[i], config[i].recommendedHeight, config[i].maxHeight);

            config[i].selectedSamples = Settings::Manager::getInt("antialiasing", "Video");
            // OpenXR requires a non-zero value
            if (config[i].selectedSamples < 1)
                config[i].selectedSamples = 1;

            Log(Debug::Verbose) << name << " resolution: Recommended x=" << config[i].recommendedWidth << ", y=" << config[i].recommendedHeight;
            Log(Debug::Verbose) << name << " resolution: Max x=" << config[i].maxWidth << ", y=" << config[i].maxHeight;
            Log(Debug::Verbose) << name << " resolution: Selected x=" << config[i].selectedWidth << ", y=" << config[i].selectedHeight;


            auto view = new VRView(name, config[i], context->getState());
            mViews[name] = view;
            auto camera = mCameras[name] = view->createCamera(i + 2, clearColor, context);
            camera->setPreDrawCallback(mPreDraw);
            camera->setFinalDrawCallback(mPostDraw);
            camera->setCullMask(~MWRender::Mask_GUI & ~MWRender::Mask_SimpleWater & ~MWRender::Mask_UpdateVisitor);
            camera->setName(name);
            if (smallFeatureCulling)
                camera->setSmallFeatureCullingPixelSize(smallFeatureCullingPixelSize);
            camera->setCullingMode(cullingMode);
            mViewer->addSlave(camera, true);
            auto* slave = mViewer->findSlaveForCamera(camera);
            slave->_updateSlaveCallback = new VRView::UpdateSlaveCallback(view);

            if (mirror)
                mMsaaResolveMirrorTexture[i].reset(new VRFramebuffer(context->getState(),
                    view->swapchain().width(),
                    view->swapchain().height(),
                    0));

            mVrShadow.configureShadowsForCamera(camera, i == 0);
        }

        if (mirror)
            mMirrorTexture.reset(new VRFramebuffer(context->getState(), mainCamera->getViewport()->width(), mainCamera->getViewport()->height(), 0));
        mViewer->setReleaseContextAtEndOfFrameHint(false);

        mMainCameraGC = mainCamera->getGraphicsContext();
        mMainCameraGC->setSwapCallback(new VRViewer::SwapBuffersCallback(this));
        mainCamera->setGraphicsContext(nullptr);
        mConfigured = true;

        Log(Debug::Verbose) << "Realized";
    }

    VRView* VRViewer::getView(std::string name)
    {
        auto it = mViews.find(name);
        if (it != mViews.end())
            return it->second.get();
        return nullptr;
    }

    void VRViewer::enableMainCamera(void)
    {
        mCameras["MainCamera"]->setGraphicsContext(mMainCameraGC);
    }

    void VRViewer::disableMainCamera(void)
    {
        mCameras["MainCamera"]->setGraphicsContext(nullptr);
    }

    void VRViewer::blitEyesToMirrorTexture(osg::GraphicsContext* gc)
    {
        if (!mMirrorTexture)
            return;

        auto* state = gc->getState();
        auto* gl = osg::GLExtensions::Get(state->getContextID(), false);

        int screenWidth = mCameras["MainCamera"]->getViewport()->width();
        int mirrorWidth = screenWidth / mMirrorTextureViews.size();
        int screenHeight = mCameras["MainCamera"]->getViewport()->height();

        // Since OpenXR does not include native support for mirror textures, we have to generate them ourselves
        // which means resolving msaa twice.
        for (unsigned i = 0; i < mMirrorTextureViews.size(); i++)
        {
            auto& resolveTexture = *mMsaaResolveMirrorTexture[i];
            resolveTexture.bindFramebuffer(gc, GL_FRAMEBUFFER_EXT);
            mViews[mMirrorTextureViews[i]]->swapchain().renderBuffer()->blit(gc, 0, 0, resolveTexture.width(), resolveTexture.height());
            mMirrorTexture->bindFramebuffer(gc, GL_FRAMEBUFFER_EXT);
            resolveTexture.blit(gc, i * mirrorWidth, 0, (i + 1) * mirrorWidth, screenHeight);
        }

        gl->glBindFramebuffer(GL_FRAMEBUFFER_EXT, 0);
        mMirrorTexture->blit(gc, 0, 0, screenWidth, screenHeight);
    }

    void
        VRViewer::SwapBuffersCallback::swapBuffersImplementation(
            osg::GraphicsContext* gc)
    {
        auto* session = Environment::get().getSession();
        session->swapBuffers(gc, *mViewer);
    }

    void
        RealizeOperation::operator()(
            osg::GraphicsContext* gc)
    {
        return Environment::get().getViewer()->realize(gc);
    }

    bool
        RealizeOperation::realized()
    {
        return Environment::get().getViewer()->realized();
    }

    void VRViewer::preDrawCallback(osg::RenderInfo& info)
    {
        auto* camera = info.getCurrentCamera();
        auto name = camera->getName();
        mViews[name]->prerenderCallback(info);
    }

    void VRViewer::postDrawCallback(osg::RenderInfo& info)
    {
        auto* camera = info.getCurrentCamera();
        auto name = camera->getName();
        auto& view = mViews[name];

        view->postrenderCallback(info);

        // This happens sometimes, i've not been able to catch it when as happens
        // to see why and how i can stop it.
        if (camera->getPreDrawCallback() != mPreDraw)
        {
            camera->setPreDrawCallback(mPreDraw);
            Log(Debug::Warning) << ("osg overwrote predraw");
        }
    }
}
