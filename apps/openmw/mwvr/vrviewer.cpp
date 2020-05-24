#include "vrviewer.hpp"
#include "vrsession.hpp"
#include "openxrmanagerimpl.hpp"
#include "openxrinputmanager.hpp"
#include "vrenvironment.hpp"
#include "Windows.h"
#include "../mwmechanics/actorutil.hpp"
#include "../mwbase/world.hpp"
#include "../mwbase/environment.hpp"
#include "../mwworld/class.hpp"
#include "../mwworld/player.hpp"
#include "../mwworld/esmstore.hpp"
#include "../mwrender/vismask.hpp"
#include <components/esm/loadrace.hpp>
#include <osg/MatrixTransform>

namespace MWVR
{

    VRViewer::VRViewer(
        osg::ref_ptr<osgViewer::Viewer> viewer)
        : mRealizeOperation(new RealizeOperation())
        , mViewer(viewer)
        , mPreDraw(new PredrawCallback(this))
        , mPostDraw(new PostdrawCallback(this))
        , mConfigured(false)
    {
        mViewer->setRealizeOperation(mRealizeOperation);
        //this->setName("OpenXRRoot");
    }

    VRViewer::~VRViewer(void)
    {
    }

    void VRViewer::traversals()
    {
        mViewer->updateTraversal();
        mViewer->renderingTraversals();
    }

    void VRViewer::realize(osg::GraphicsContext* context)
    {
        std::unique_lock<std::mutex> lock(mMutex);
        
        if (mConfigured)
        {
            return;
        }

        if (!context->isCurrent())
        if (!context->makeCurrent())
        {
            throw std::logic_error("VRViewer::configure() failed to make graphics context current.");
            return;
        }

        auto mainCamera = mCameras["MainCamera"] = mViewer->getCamera();
        mainCamera->setName("Main");
        mainCamera->setInitialDrawCallback(new VRView::InitialDrawCallback());

        osg::Vec4 clearColor = mainCamera->getClearColor();

        auto* xr = Environment::get().getManager();
        if (!xr->realized())
            xr->realize(context);

        xr->handleEvents();

        OpenXRSwapchain::Config leftConfig;
        leftConfig.width = xr->impl().mConfigViews[(int)Side::LEFT_HAND].recommendedImageRectWidth;
        leftConfig.height = xr->impl().mConfigViews[(int)Side::LEFT_HAND].recommendedImageRectHeight;
        leftConfig.samples = xr->impl().mConfigViews[(int)Side::LEFT_HAND].recommendedSwapchainSampleCount;
        OpenXRSwapchain::Config rightConfig;
        rightConfig.width = xr->impl().mConfigViews[(int)Side::RIGHT_HAND].recommendedImageRectWidth;
        rightConfig.height = xr->impl().mConfigViews[(int)Side::RIGHT_HAND].recommendedImageRectHeight;
        rightConfig.samples = xr->impl().mConfigViews[(int)Side::RIGHT_HAND].recommendedSwapchainSampleCount;

        auto leftView = new VRView("LeftEye", leftConfig, context->getState());
        auto rightView = new VRView("RightEye", rightConfig, context->getState());

        mViews["LeftEye"] = leftView;
        mViews["RightEye"] = rightView;

        auto leftCamera = mCameras["LeftEye"] = leftView->createCamera(0, clearColor, context);
        auto rightCamera = mCameras["RightEye"] = rightView->createCamera(1, clearColor, context);

        leftCamera->setPreDrawCallback(mPreDraw);
        rightCamera->setPreDrawCallback(mPreDraw);

        leftCamera->setFinalDrawCallback(mPostDraw);
        rightCamera->setFinalDrawCallback(mPostDraw);

        // Stereo cameras should only draw the scene
        leftCamera->setCullMask(~MWRender::Mask_GUI & ~MWRender::Mask_SimpleWater & ~MWRender::Mask_UpdateVisitor);
        rightCamera->setCullMask(~MWRender::Mask_GUI & ~MWRender::Mask_SimpleWater & ~MWRender::Mask_UpdateVisitor);

        leftCamera->setName("LeftEye");
        rightCamera->setName("RightEye");


        osg::Camera::CullingMode cullingMode = osg::Camera::DEFAULT_CULLING|osg::Camera::FAR_PLANE_CULLING;

        if (!Settings::Manager::getBool("small feature culling", "Camera"))
            cullingMode &= ~(osg::CullStack::SMALL_FEATURE_CULLING);
        else
        {
            auto smallFeatureCullingPixelSize = Settings::Manager::getFloat("small feature culling pixel size", "Camera");
            leftCamera->setSmallFeatureCullingPixelSize(smallFeatureCullingPixelSize);
            rightCamera->setSmallFeatureCullingPixelSize(smallFeatureCullingPixelSize);
            cullingMode |= osg::CullStack::SMALL_FEATURE_CULLING;
        }
        leftCamera->setCullingMode(cullingMode);
        rightCamera->setCullingMode(cullingMode);

        mViewer->addSlave(leftCamera, true);
        mViewer->addSlave(rightCamera, true);

        mViewer->setReleaseContextAtEndOfFrameHint(false);


        OpenXRSwapchain::Config config;
        config.width = mainCamera->getViewport()->width();
        config.height = mainCamera->getViewport()->height();
        config.samples = 1;

        // Mirror texture doesn't have to be an OpenXR swapchain.
        // It was just convenient at the time.
        mMirrorTextureSwapchain.reset(new OpenXRSwapchain(context->getState(), config));

        mViewer->getSlave(0)._updateSlaveCallback = new VRView::UpdateSlaveCallback(leftView, context);
        mViewer->getSlave(1)._updateSlaveCallback = new VRView::UpdateSlaveCallback(rightView, context);

        mMainCameraGC = mainCamera->getGraphicsContext();
        mMainCameraGC->setSwapCallback(new VRViewer::SwapBuffersCallback(this));
        mainCamera->setGraphicsContext(nullptr);
        mConfigured = true;

        Log(Debug::Verbose) << "Realized";
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
        //includeMenu = false;
        mMirrorTextureSwapchain->beginFrame(gc);

        int mirror_width = mMirrorTextureSwapchain->width() / 2;


        mViews["RightEye"]->swapchain().renderBuffer()->blit(gc, 0, 0, mirror_width, mMirrorTextureSwapchain->height());
        mViews["LeftEye"]->swapchain().renderBuffer()->blit(gc, mirror_width, 0, 2 * mirror_width, mMirrorTextureSwapchain->height());

        auto* state = gc->getState();
        auto* gl = osg::GLExtensions::Get(state->getContextID(), false);

        mMirrorTextureSwapchain->endFrame(gc);



        gl->glBindFramebuffer(GL_FRAMEBUFFER_EXT, 0);
        mMirrorTextureSwapchain->renderBuffer()->blit(gc, 0, 0, mMirrorTextureSwapchain->width(), mMirrorTextureSwapchain->height());

    }

    void
        VRViewer::SwapBuffersCallback::swapBuffersImplementation(
            osg::GraphicsContext* gc)
    {
        auto* session = Environment::get().getSession();
        session->swapBuffers(gc, *mViewer);
        gc->swapBuffersImplementation();
    }

    void VRViewer::swapBuffers(osg::GraphicsContext* gc)
    {
        if (!mConfigured)
            return;

        Timer timer("VRViewer::SwapBuffers");

        for (auto& view : mViews)
            view.second->swapBuffers(gc);

        timer.checkpoint("Views");

        blitEyesToMirrorTexture(gc);

        gc->swapBuffersImplementation();
    }

    void
        VRViewer::RealizeOperation::operator()(
            osg::GraphicsContext* gc)
    {
        OpenXRManager::RealizeOperation::operator()(gc);

        Environment::get().getViewer()->realize(gc);
    }

    bool
        VRViewer::RealizeOperation::realized()
    {
        return Environment::get().getViewer()->realized();
    }

    void VRViewer::preDrawCallback(osg::RenderInfo& info)
    {
        auto* camera = info.getCurrentCamera();
        auto name = camera->getName();
        auto& view = mViews[name];

        if (name == "LeftEye")
            Environment::get().getSession()->advanceFramePhase();

        view->prerenderCallback(info);
    }

    void VRViewer::postDrawCallback(osg::RenderInfo& info)
    {
        auto* camera = info.getCurrentCamera();
        auto name = camera->getName();
        auto& view = mViews[name];

        view->postrenderCallback(info);

        // OSG will sometimes overwrite the predraw callback.
        if (camera->getPreDrawCallback() != mPreDraw)
        {
            camera->setPreDrawCallback(mPreDraw);
            Log(Debug::Warning) << ("osg overwrote predraw");
        }

        //if(mDelay && name == "RightEye")
        //    std::this_thread::sleep_for(std::chrono::milliseconds(25));
    }
}
