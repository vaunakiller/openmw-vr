#include "openxrviewer.hpp"
#include "openxrmanagerimpl.hpp"
#include "Windows.h"
#include "../mwrender/vismask.hpp"

namespace MWVR
{

    OpenXRViewer::OpenXRViewer(
        osg::ref_ptr<OpenXRManager> XR,
        osg::ref_ptr<osgViewer::Viewer> viewer,
        float metersPerUnit)
        : mXR(XR)
        , mRealizeOperation(new RealizeOperation(XR, this))
        , mViewer(viewer)
        , mMetersPerUnit(metersPerUnit)
        , mConfigured(false)
        , mCompositionLayerProjectionViews(2, {XR_TYPE_COMPOSITION_LAYER_PROJECTION_VIEW})
        , mXRSession(nullptr)
        , mPreDraw(new PredrawCallback(this))
        , mPostDraw(new PostdrawCallback(this))
    {
        mViewer->setRealizeOperation(mRealizeOperation);
        mCompositionLayerProjectionViews[0].pose.orientation.w = 1;
        mCompositionLayerProjectionViews[1].pose.orientation.w = 1;
    }

    OpenXRViewer::~OpenXRViewer(void)
    {
    }

    void 
        OpenXRViewer::traverse(
            osg::NodeVisitor& visitor)
    {
        Log(Debug::Verbose) << "Traversal";
        if (mRealizeOperation->realized())
            osg::Group::traverse(visitor);
    }

    const XrCompositionLayerBaseHeader*
        OpenXRViewer::layer()
    {
        return reinterpret_cast<XrCompositionLayerBaseHeader*>(mLayer.get());
    }

    void OpenXRViewer::traversals()
    {
        //Log(Debug::Verbose) << "Pre-Update";
        mXR->handleEvents();
        mViewer->updateTraversal();
        //Log(Debug::Verbose) << "Post-Update";
        //Log(Debug::Verbose) << "Pre-Rendering";
        mViewer->renderingTraversals();
        //Log(Debug::Verbose) << "Post-Rendering";
    }

    void OpenXRViewer::realize(osg::GraphicsContext* context)
    {
        std::unique_lock<std::mutex> lock(mMutex);

        if (mConfigured)
            return;

        if (!context->isCurrent())
        if (!context->makeCurrent())
        {
            throw std::logic_error("OpenXRViewer::configure() failed to make graphics context current.");
            return;
        }

        

        auto mainCamera = mCameras["MainCamera"] = mViewer->getCamera();
        mainCamera->setName("Main");
        mainCamera->setInitialDrawCallback(new OpenXRView::InitialDrawCallback());

        // Use the main camera to render any GUI to the OpenXR GUI quad's swapchain.
        // (When swapping the window buffer we'll blit the mirror texture to it instead.)
        mainCamera->setCullMask(MWRender::Mask_GUI);

        osg::Vec4 clearColor = mainCamera->getClearColor();

        if (!mXR->realized())
            mXR->realize(context);

        OpenXRSwapchain::Config leftConfig;
        leftConfig.width = mXR->impl().mConfigViews[(int)Chirality::LEFT_HAND].recommendedImageRectWidth;
        leftConfig.height = mXR->impl().mConfigViews[(int)Chirality::LEFT_HAND].recommendedImageRectHeight;
        leftConfig.samples = mXR->impl().mConfigViews[(int)Chirality::LEFT_HAND].recommendedSwapchainSampleCount;
        OpenXRSwapchain::Config rightConfig;
        rightConfig.width = mXR->impl().mConfigViews[(int)Chirality::RIGHT_HAND].recommendedImageRectWidth;
        rightConfig.height = mXR->impl().mConfigViews[(int)Chirality::RIGHT_HAND].recommendedImageRectHeight;
        rightConfig.samples = mXR->impl().mConfigViews[(int)Chirality::RIGHT_HAND].recommendedSwapchainSampleCount;

        auto leftView = new OpenXRWorldView(mXR, "LeftEye", context->getState(), leftConfig, mMetersPerUnit);
        auto rightView = new OpenXRWorldView(mXR, "RightEye", context->getState(), rightConfig, mMetersPerUnit);

        mViews["LeftEye"] = leftView;
        mViews["RightEye"] = rightView;

        auto leftCamera = mCameras["LeftEye"] = leftView->createCamera(0, clearColor, context);
        auto rightCamera = mCameras["RightEye"] = rightView->createCamera(1, clearColor, context);

        leftCamera->setPreDrawCallback(mPreDraw);
        rightCamera->setPreDrawCallback(mPreDraw);

        leftCamera->setPostDrawCallback(mPostDraw);
        rightCamera->setPostDrawCallback(mPostDraw);

        // Stereo cameras should only draw the scene (AR layers should later add minimap, health, etc.)
        leftCamera->setCullMask(~MWRender::Mask_GUI);
        rightCamera->setCullMask(~MWRender::Mask_GUI);

        leftCamera->setName("LeftEye");
        rightCamera->setName("RightEye");

        mViewer->addSlave(leftCamera, leftView->projectionMatrix(), leftView->viewMatrix(), true);
        mViewer->addSlave(rightCamera, rightView->projectionMatrix(), rightView->viewMatrix(), true);


        mViewer->setLightingMode(osg::View::SKY_LIGHT);
        mViewer->setReleaseContextAtEndOfFrameHint(false);

        mXRInput.reset(new OpenXRInputManager(mXR));

        mCompositionLayerProjectionViews[0].subImage = leftView->swapchain().subImage();
        mCompositionLayerProjectionViews[1].subImage = rightView->swapchain().subImage();


        OpenXRSwapchain::Config config;
        config.width = mainCamera->getViewport()->width();
        config.height = mainCamera->getViewport()->height();
        config.samples = 1;

        // Mirror texture doesn't have to be an OpenXR swapchain.
        // It's just convenient.
        mMirrorTextureSwapchain.reset(new OpenXRSwapchain(mXR, context->getState(), config));

        auto menuView = new OpenXRMenu(mXR, config, context->getState(), "MainMenu", osg::Vec2(1.f, 1.f));
        mViews["MenuView"] = menuView;

        auto menuCamera = mCameras["MenuView"] = menuView->createCamera(2, clearColor, context);
        menuCamera->setCullMask(MWRender::Mask_GUI);
        menuCamera->setName("MenuView");
        menuCamera->setPreDrawCallback(mPreDraw);
        menuCamera->setPostDrawCallback(mPostDraw);

        mViewer->addSlave(menuCamera, true);

        mXRSession.reset(new OpenXRSession(mXR));
        mViewer->getSlave(0)._updateSlaveCallback = new OpenXRWorldView::UpdateSlaveCallback(mXR, mXRSession.get(), leftView, context);
        mViewer->getSlave(1)._updateSlaveCallback = new OpenXRWorldView::UpdateSlaveCallback(mXR, mXRSession.get(), rightView, context);

        mainCamera->getGraphicsContext()->setSwapCallback(new OpenXRViewer::SwapBuffersCallback(this));
        mainCamera->setGraphicsContext(nullptr);
        mXRSession->setLayer(OpenXRLayerStack::WORLD_VIEW_LAYER, this);
        mXRSession->setLayer(OpenXRLayerStack::MENU_VIEW_LAYER, dynamic_cast<OpenXRLayer*>(mViews["MenuView"].get()));
        mConfigured = true;

    }

    void OpenXRViewer::blitEyesToMirrorTexture(osg::GraphicsContext* gc)
    {
        mMirrorTextureSwapchain->beginFrame(gc);
        mViews["LeftEye"]->swapchain().renderBuffer()->blit(gc, 0, 0, mMirrorTextureSwapchain->width() / 2, mMirrorTextureSwapchain->height());
        mViews["RightEye"]->swapchain().renderBuffer()->blit(gc, mMirrorTextureSwapchain->width() / 2, 0, mMirrorTextureSwapchain->width(), mMirrorTextureSwapchain->height());

        //mXRMenu->swapchain().current()->blit(gc, 0, 0, mMirrorTextureSwapchain->width() / 2, mMirrorTextureSwapchain->height());
    }

    void
        OpenXRViewer::SwapBuffersCallback::swapBuffersImplementation(
            osg::GraphicsContext* gc)
    {
        mViewer->mXRSession->swapBuffers(gc);
    }

    void OpenXRViewer::swapBuffers(osg::GraphicsContext* gc)
    {

        if (!mConfigured)
            return;

        auto* state = gc->getState();
        auto* gl = osg::GLExtensions::Get(state->getContextID(), false);

        ////// NEW SYSTEM
        Timer timer("OpenXRViewer::SwapBuffers");
        mViews["LeftEye"]->swapBuffers(gc);
        mViews["RightEye"]->swapBuffers(gc);
        timer.checkpoint("Views");

        auto eyePoses = mXRSession->predictedPoses().eye;
        auto leftEyePose = toXR(mViews["LeftEye"]->predictedPose());
        auto rightEyePose = toXR(mViews["RightEye"]->predictedPose());
        mCompositionLayerProjectionViews[0].pose = leftEyePose;
        mCompositionLayerProjectionViews[1].pose = rightEyePose;
        timer.checkpoint("Poses");

        // TODO: Keep track of these in the session too.
        auto stageViews = mXR->impl().getPredictedViews(mXRSession->predictedDisplayTime(), TrackedSpace::STAGE);
        mCompositionLayerProjectionViews[0].fov = stageViews[0].fov;
        mCompositionLayerProjectionViews[1].fov = stageViews[1].fov;
        timer.checkpoint("Fovs");


        if (!mLayer)
        {
            mLayer.reset(new XrCompositionLayerProjection);
            mLayer->type = XR_TYPE_COMPOSITION_LAYER_PROJECTION;
            mLayer->space = mXR->impl().mReferenceSpaceStage;
            mLayer->viewCount = 2;
            mLayer->views = mCompositionLayerProjectionViews.data();
        }
    }

    void
        OpenXRViewer::RealizeOperation::operator()(
            osg::GraphicsContext* gc)
    {
        OpenXRManager::RealizeOperation::operator()(gc);
        mViewer->realize(gc);
    }

    bool
        OpenXRViewer::RealizeOperation::realized()
    {
        return mViewer->realized();
    }

    void OpenXRViewer::preDrawCallback(osg::RenderInfo& info)
    {
        auto* camera = info.getCurrentCamera();
        auto name = camera->getName();
        auto& view = mViews[name];

        if (name == "LeftEye")
        {
            mXR->beginFrame();
            auto& poses = mXRSession->predictedPoses();
            auto menuPose = poses.head[(int)TrackedSpace::STAGE];
            mViews["MenuView"]->setPredictedPose(menuPose);
        }

        view->prerenderCallback(info);
    }

    void OpenXRViewer::postDrawCallback(osg::RenderInfo& info)
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
    }
}
