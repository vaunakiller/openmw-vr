#include "vrviewer.hpp"

#include "openxrmanagerimpl.hpp"
#include "openxrswapchain.hpp"
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
        , mConfigured(false)
        , mMsaaResolveMirrorTexture{}
        , mMirrorTexture{ nullptr }
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
        return recommended;
    }

    static VRViewer::MirrorTextureEye mirrorTextureEyeFromString(const std::string& str)
    {
        if (Misc::StringUtils::ciEqual(str, "left"))
            return VRViewer::MirrorTextureEye::Left;
        if (Misc::StringUtils::ciEqual(str, "right"))
            return VRViewer::MirrorTextureEye::Right;
        if (Misc::StringUtils::ciEqual(str, "both"))
            return VRViewer::MirrorTextureEye::Both;
        return VRViewer::MirrorTextureEye::Both;
    }

    void VRViewer::InitialDrawCallback::operator()(osg::RenderInfo& renderInfo) const
    {
        const auto& name = renderInfo.getCurrentCamera()->getName();
        if (name == "LeftEye")
            Environment::get().getSession()->beginPhase(VRSession::FramePhase::Draw);
    
        osg::GraphicsOperation* graphicsOperation = renderInfo.getCurrentCamera()->getRenderer();
        osgViewer::Renderer* renderer = dynamic_cast<osgViewer::Renderer*>(graphicsOperation);
        if (renderer != nullptr)
        {
            // Disable normal OSG FBO camera setup
            renderer->setCameraRequiresSetUp(false);
        }
    }

    class CullCallback : public osg::NodeCallback
    {
        void operator()(osg::Node* node, osg::NodeVisitor* nv)
        {
            Environment::get().getSession()->beginPhase(VRSession::FramePhase::Cull);
            traverse(node, nv);
        }
    };

    void VRViewer::realize(osg::GraphicsContext* context)
    {
        std::unique_lock<std::mutex> lock(mMutex);

        if (mConfigured)
        {
            return;
        }

        // Give the main camera an initial draw callback that disables camera setup (we don't want it)
        auto mainCamera = mViewer->getCamera();
        mainCamera->setName("Main");
        mainCamera->setInitialDrawCallback(new InitialDrawCallback());

        auto* xr = Environment::get().getManager();
        xr->realize(context);

        // Run through initial events to start session
        // For the rest of runtime this is handled by vrsession
        xr->handleEvents();

        // Set up swapchain config
        auto config = xr->getRecommendedSwapchainConfig();

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

            config[i].name = sViewNames[i];

            mSubImages[i].width = config[i].selectedWidth;
            mSubImages[i].height = config[i].selectedHeight;
            if (i > 0)
            {
                mSubImages[i].x = mSubImages[i - 1].x + mSubImages[i - 1].width;
                mSubImages[i].y = mSubImages[i - 1].y + mSubImages[i - 1].height;
            }
            else
            {
                mSubImages[i].x = mSubImages[i].y = 0;
            }
        }

        mSwapchainConfig.name = "Main";
        mSwapchainConfig.selectedWidth = config[0].selectedWidth + config[1].selectedWidth;
        mSwapchainConfig.selectedHeight = std::max(config[0].selectedHeight, config[1].selectedHeight);
        mSwapchainConfig.selectedSamples = std::max(config[0].selectedSamples, config[1].selectedSamples);

        mSwapchain.reset(new OpenXRSwapchain(context->getState(), mSwapchainConfig));

        mSubImages[0].swapchain = mSubImages[1].swapchain = mSwapchain.get();

        mViewer->setReleaseContextAtEndOfFrameHint(false);

        setupMirrorTexture();

        mainCamera->getGraphicsContext()->setSwapCallback(new VRViewer::SwapBuffersCallback(this));
        mainCamera->setPreDrawCallback(mPreDraw);
        mainCamera->setPostDrawCallback(mPostDraw);
        mainCamera->setCullCallback(new CullCallback);
        mConfigured = true;

        Log(Debug::Verbose) << "Realized";
    }

    void VRViewer::setupMirrorTexture()
    {
        mMirrorTextureEnabled = Settings::Manager::getBool("mirror texture", "VR");
        mMirrorTextureEye = mirrorTextureEyeFromString(Settings::Manager::getString("mirror texture eye", "VR"));
        mFlipMirrorTextureOrder = Settings::Manager::getBool("flip mirror texture order", "VR");
        mMirrorTextureShouldBeCleanedUp = true;

        mMirrorTextureViews.clear();
        if (mMirrorTextureEye == MirrorTextureEye::Left || mMirrorTextureEye == MirrorTextureEye::Both)
            mMirrorTextureViews.push_back(sViewNames[(int)Side::LEFT_SIDE]);
        if (mMirrorTextureEye == MirrorTextureEye::Right || mMirrorTextureEye == MirrorTextureEye::Both)
            mMirrorTextureViews.push_back(sViewNames[(int)Side::RIGHT_SIDE]);
        if (mFlipMirrorTextureOrder)
            std::reverse(mMirrorTextureViews.begin(), mMirrorTextureViews.end());
        // TODO: If mirror is false either hide the window or paste something meaningful into it.
        // E.g. Fanart of Dagoth UR wearing a VR headset
    }

    void VRViewer::processChangedSettings(const std::set<std::pair<std::string, std::string>>& changed)
    {
        bool mirrorTextureChanged = false;
        for (Settings::CategorySettingVector::const_iterator it = changed.begin(); it != changed.end(); ++it)
        {
            if (it->first == "VR" && it->second == "mirror texture")
            {
                mirrorTextureChanged = true;
            }
            if (it->first == "VR" && it->second == "mirror texture eye")
            {
                mirrorTextureChanged = true;
            }
            if (it->first == "VR" && it->second == "flip mirror texture order")
            {
                mirrorTextureChanged = true;
            }
        }

        if (mirrorTextureChanged)
            setupMirrorTexture();
    }

    SubImage VRViewer::subImage(Side side)
    {
        return mSubImages[static_cast<int>(side)];
    }

    void VRViewer::blitEyesToMirrorTexture(osg::GraphicsContext* gc)
    {
        //if (mMirrorTextureShouldBeCleanedUp)
        //{
        //    mMirrorTexture.reset(nullptr);
        //    mMsaaResolveMirrorTexture.clear();
        //    mMirrorTextureShouldBeCleanedUp = false;
        //}
        //if (!mMirrorTextureEnabled)
        //    return;
        //if (!mMirrorTexture)
        //{
        //    mMirrorTexture.reset(new VRFramebuffer(gc->getState(), mCameras["MainCamera"]->getViewport()->width(), mCameras["MainCamera"]->getViewport()->height(), 0));
        //}

        auto* state = gc->getState();
        auto* gl = osg::GLExtensions::Get(state->getContextID(), false);

        //auto mainCamera = mViewer->getCamera();
        //int screenWidth = mainCamera->getViewport()->width();
        //int mirrorWidth = screenWidth / mMirrorTextureViews.size();
        //int screenHeight = mainCamera->getViewport()->height();

        //// Since OpenXR does not include native support for mirror textures, we have to generate them ourselves
        //// which means resolving msaa twice.
        //for (unsigned i = 0; i < mMirrorTextureViews.size(); i++)
        //{
        //    if(!mMsaaResolveMirrorTexture)
        //        mMsaaResolveMirrorTexture.reset(new VRFramebuffer(gc->getState(),
        //            mSwapchain->width(),
        //            mSwapchain->height(),
        //            0));

        //    auto& resolveTexture = *mMsaaResolveMirrorTexture[mMirrorTextureViews[i]];
        //    resolveTexture.bindFramebuffer(gc, GL_FRAMEBUFFER_EXT);
        //    view->swapchain().renderBuffer()->blit(gc, 0, 0, resolveTexture.width(), resolveTexture.height());
        //    mMirrorTexture->bindFramebuffer(gc, GL_FRAMEBUFFER_EXT);
        //    resolveTexture.blit(gc, i * mirrorWidth, 0, (i + 1) * mirrorWidth, screenHeight);
        //}

        gl->glBindFramebuffer(GL_FRAMEBUFFER_EXT, 0);
        //mMirrorTexture->blit(gc, 0, 0, screenWidth, screenHeight);

        mSwapchain->endFrame(gc);
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
        if(Environment::get().getSession()->getFrame(VRSession::FramePhase::Draw)->mShouldRender)
            mSwapchain->beginFrame(info.getState()->getGraphicsContext());
    }

    void VRViewer::postDrawCallback(osg::RenderInfo& info)
    {
        auto* camera = info.getCurrentCamera();
        auto name = camera->getName();

        // This happens sometimes, i've not been able to catch it when as happens
        // to see why and how i can stop it.
        if (camera->getPreDrawCallback() != mPreDraw)
        {
            camera->setPreDrawCallback(mPreDraw);
            Log(Debug::Warning) << ("osg overwrote predraw");
        }
    }
}
