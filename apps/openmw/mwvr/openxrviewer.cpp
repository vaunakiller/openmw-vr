#include "openxrviewer.hpp"
#include "openxrsession.hpp"
#include "openxrmanagerimpl.hpp"
#include "openxrinputmanager.hpp"
#include "vrenvironment.hpp"
#include "Windows.h"
#include "../mwrender/vismask.hpp"
#include "../mwmechanics/actorutil.hpp"
#include "../mwbase/world.hpp"
#include "../mwbase/environment.hpp"
#include "../mwworld/class.hpp"
#include "../mwworld/player.hpp"
#include "../mwworld/esmstore.hpp"
#include <components/esm/loadrace.hpp>
#include <osg/MatrixTransform>

namespace MWVR
{

    OpenXRViewer::OpenXRViewer(
        osg::ref_ptr<osgViewer::Viewer> viewer)
        : osg::Group()
        , mCompositionLayerProjectionViews(2, {XR_TYPE_COMPOSITION_LAYER_PROJECTION_VIEW})
        , mRealizeOperation(new RealizeOperation())
        , mViewer(viewer)
        , mPreDraw(new PredrawCallback(this))
        , mPostDraw(new PostdrawCallback(this))
        , mConfigured(false)
    {
        mViewer->setRealizeOperation(mRealizeOperation);
        mCompositionLayerProjectionViews[0].pose.orientation.w = 1;
        mCompositionLayerProjectionViews[1].pose.orientation.w = 1;
        this->setName("OpenXRRoot");

        this->setUserData(new TrackedNodeUpdateCallback(this));

        mLeftHandTransform->setName("tracker l hand");
        mLeftHandTransform->setUpdateCallback(new TrackedNodeUpdateCallback(this));
        this->addChild(mLeftHandTransform);

        mRightHandTransform->setName("tracker r hand");
        mRightHandTransform->setUpdateCallback(new TrackedNodeUpdateCallback(this));
        this->addChild(mRightHandTransform);
    }

    OpenXRViewer::~OpenXRViewer(void)
    {
    }

    void 
        OpenXRViewer::traverse(
            osg::NodeVisitor& visitor)
    {
        if (mRealizeOperation->realized())
            osg::Group::traverse(visitor);
    }

    const XrCompositionLayerBaseHeader*
        OpenXRViewer::layer()
    {
        // Does not check for visible, since this should always render
        return reinterpret_cast<XrCompositionLayerBaseHeader*>(mLayer.get());
    }

    void OpenXRViewer::traversals()
    {
        auto* xr = Environment::get().getManager();
        xr->handleEvents();
        mViewer->updateTraversal();
        mViewer->renderingTraversals();
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

        auto* xr = Environment::get().getManager();
        if (!xr->realized())
            xr->realize(context);

        auto* session = Environment::get().getSession();

        OpenXRSwapchain::Config leftConfig;
        leftConfig.width = xr->impl().mConfigViews[(int)Side::LEFT_HAND].recommendedImageRectWidth;
        leftConfig.height = xr->impl().mConfigViews[(int)Side::LEFT_HAND].recommendedImageRectHeight;
        leftConfig.samples = xr->impl().mConfigViews[(int)Side::LEFT_HAND].recommendedSwapchainSampleCount;
        OpenXRSwapchain::Config rightConfig;
        rightConfig.width = xr->impl().mConfigViews[(int)Side::RIGHT_HAND].recommendedImageRectWidth;
        rightConfig.height = xr->impl().mConfigViews[(int)Side::RIGHT_HAND].recommendedImageRectHeight;
        rightConfig.samples = xr->impl().mConfigViews[(int)Side::RIGHT_HAND].recommendedSwapchainSampleCount;

        auto leftView = new OpenXRWorldView(xr, "LeftEye", context->getState(), leftConfig);
        auto rightView = new OpenXRWorldView(xr, "RightEye", context->getState(), rightConfig);

        mViews["LeftEye"] = leftView;
        mViews["RightEye"] = rightView;

        auto leftCamera = mCameras["LeftEye"] = leftView->createCamera(0, clearColor, context);
        auto rightCamera = mCameras["RightEye"] = rightView->createCamera(1, clearColor, context);

        leftCamera->setPreDrawCallback(mPreDraw);
        rightCamera->setPreDrawCallback(mPreDraw);

        //leftCamera->setPostDrawCallback(mPostDraw);
        //rightCamera->setPostDrawCallback(mPostDraw);
        leftCamera->setFinalDrawCallback(mPostDraw);
        rightCamera->setFinalDrawCallback(mPostDraw);

        // Stereo cameras should only draw the scene (AR layers should later add minimap, health, etc.)
        leftCamera->setCullMask(~MWRender::Mask_GUI);
        rightCamera->setCullMask(~MWRender::Mask_GUI);

        leftCamera->setName("LeftEye");
        rightCamera->setName("RightEye");

        mViewer->addSlave(leftCamera, leftView->projectionMatrix(), leftView->viewMatrix(), true);
        mViewer->addSlave(rightCamera, rightView->projectionMatrix(), rightView->viewMatrix(), true);


        mViewer->setLightingMode(osg::View::SKY_LIGHT);
        mViewer->setReleaseContextAtEndOfFrameHint(false);

        mCompositionLayerProjectionViews[0].subImage = leftView->swapchain().subImage();
        mCompositionLayerProjectionViews[1].subImage = rightView->swapchain().subImage();


        OpenXRSwapchain::Config config;
        config.width = mainCamera->getViewport()->width();
        config.height = mainCamera->getViewport()->height();
        config.samples = 1;

        // Mirror texture doesn't have to be an OpenXR swapchain.
        // It's just convenient.
        mMirrorTextureSwapchain.reset(new OpenXRSwapchain(xr, context->getState(), config));

        //mViewer->addSlave(menuCamera, true);
        mViewer->getSlave(0)._updateSlaveCallback = new OpenXRWorldView::UpdateSlaveCallback(xr, session, leftView, context);
        mViewer->getSlave(1)._updateSlaveCallback = new OpenXRWorldView::UpdateSlaveCallback(xr, session, rightView, context);

        mainCamera->getGraphicsContext()->setSwapCallback(new OpenXRViewer::SwapBuffersCallback(this));
        mainCamera->setGraphicsContext(nullptr);
        session->setLayer(OpenXRLayerStack::WORLD_VIEW_LAYER, this);
        mConfigured = true;

    }

    void OpenXRViewer::blitEyesToMirrorTexture(osg::GraphicsContext* gc, bool includeMenu)
    {
        //includeMenu = false;
        mMirrorTextureSwapchain->beginFrame(gc);

        int mirror_width = 0;
        if(includeMenu)
            mirror_width = mMirrorTextureSwapchain->width() / 3;
        else
            mirror_width = mMirrorTextureSwapchain->width() / 2;


        mViews["RightEye"]->swapchain().renderBuffer()->blit(gc, 0, 0, mirror_width, mMirrorTextureSwapchain->height());
        mViews["LeftEye"]->swapchain().renderBuffer()->blit(gc, mirror_width, 0, 2 * mirror_width, mMirrorTextureSwapchain->height());

        auto* state = gc->getState();
        auto* gl = osg::GLExtensions::Get(state->getContextID(), false);

        if (includeMenu)
        {
            auto menuManager = Environment::get().getMenuManager();
            if (menuManager)
            {
                auto menu = menuManager->getMenu();
                if (menu)
                {
                    auto texture = menu->menuTexture();
                    if (texture)
                    {
                        auto textureObject = texture->getTextureObject(state->getContextID());
                        if (textureObject)
                        {
                            auto textureId = textureObject->id();

                            Log(Debug::Verbose) << "texture id: " << textureId;

                            GLuint fbo = 0;
                            gl->glGenFramebuffers(1, &fbo);

                            gl->glBindFramebuffer(GL_READ_FRAMEBUFFER_EXT, fbo);
                            gl->glFramebufferTexture2D(GL_READ_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT0_EXT, GL_TEXTURE_2D, textureId, 0);
                            gl->glBlitFramebuffer(0, 0, texture->getTextureWidth(), texture->getTextureHeight(), 2 * mirror_width, 0, 3 * mirror_width, mMirrorTextureSwapchain->height(), GL_COLOR_BUFFER_BIT, GL_NEAREST);
                            gl->glFramebufferTexture2D(GL_READ_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT0_EXT, GL_TEXTURE_2D, 0, 0);
                            gl->glBindFramebuffer(GL_READ_FRAMEBUFFER_EXT, 0);

                            gl->glDeleteFramebuffers(1, &fbo);
                        }
                        else
                        {
                            Log(Debug::Warning) << "Texture object was null";
                        }
                    }
                }
            }
        }

        mMirrorTextureSwapchain->endFrame(gc);



        gl->glBindFramebuffer(GL_FRAMEBUFFER_EXT, 0);
        mMirrorTextureSwapchain->renderBuffer()->blit(gc, 0, 0, mMirrorTextureSwapchain->width(), mMirrorTextureSwapchain->height());
    }

    void
        OpenXRViewer::SwapBuffersCallback::swapBuffersImplementation(
            osg::GraphicsContext* gc)
    {
        auto* session = Environment::get().getSession();
        session->swapBuffers(gc);
    }

    void OpenXRViewer::swapBuffers(osg::GraphicsContext* gc)
    {

        if (!mConfigured)
            return;

        auto* session = Environment::get().getSession();
        auto* xr = Environment::get().getManager();

        Timer timer("OpenXRViewer::SwapBuffers");

        auto leftView = mViews["LeftEye"];
        auto rightView = mViews["RightEye"];

        leftView->swapBuffers(gc);
        rightView->swapBuffers(gc);
        timer.checkpoint("Views");

        mCompositionLayerProjectionViews[0].pose = toXR(session->predictedPoses().eye[(int)TrackedSpace::STAGE][(int)Side::LEFT_HAND]);
        mCompositionLayerProjectionViews[1].pose = toXR(session->predictedPoses().eye[(int)TrackedSpace::STAGE][(int)Side::RIGHT_HAND]);

        timer.checkpoint("Poses");

        // TODO: Keep track of these in the session too.
        auto stageViews = xr->impl().getPredictedViews(xr->impl().frameState().predictedDisplayTime, TrackedSpace::STAGE);
        mCompositionLayerProjectionViews[0].fov = stageViews[0].fov;
        mCompositionLayerProjectionViews[1].fov = stageViews[1].fov;
        timer.checkpoint("Fovs");


        if (!mLayer)
        {
            mLayer.reset(new XrCompositionLayerProjection);
            mLayer->type = XR_TYPE_COMPOSITION_LAYER_PROJECTION;
            mLayer->space = xr->impl().mReferenceSpaceStage;
            mLayer->viewCount = 2;
            mLayer->views = mCompositionLayerProjectionViews.data();
        }

        blitEyesToMirrorTexture(gc, false);

        gc->swapBuffersImplementation();
    }

    void
        OpenXRViewer::RealizeOperation::operator()(
            osg::GraphicsContext* gc)
    {
        OpenXRManager::RealizeOperation::operator()(gc);

        Environment::get().getViewer()->realize(gc);
    }

    bool
        OpenXRViewer::RealizeOperation::realized()
    {
        return Environment::get().getViewer()->realized();
    }

    void OpenXRViewer::preDrawCallback(osg::RenderInfo& info)
    {
        auto* camera = info.getCurrentCamera();
        auto name = camera->getName();
        auto& view = mViews[name];

        if (name == "LeftEye")
        {
            auto* xr = Environment::get().getManager();
            if (xr->sessionRunning())
            {
                xr->beginFrame();
            }
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

    void OpenXRViewer::updateTransformNode(osg::Object* object, osg::Object* data)
    {
        auto* hand_transform = dynamic_cast<SceneUtil::PositionAttitudeTransform*>(object);
        if (!hand_transform)
        {
            Log(Debug::Error) << "Update node was not PositionAttitudeTransform";
            return;
        }

        auto* xr = Environment::get().getManager();
        auto* session = Environment::get().getSession();
        auto& poses = session->predictedPoses();
        auto handPosesStage = poses.hands[(int)TrackedSpace::STAGE];
        int side = (int)Side::LEFT_HAND;
        if (hand_transform->getName() == "tracker r hand")
        {
            side = (int)Side::RIGHT_HAND;
        }

        MWVR::Pose handStage = handPosesStage[side];
        MWVR::Pose headStage = poses.head[(int)TrackedSpace::STAGE];
        xr->playerScale(handStage);
        xr->playerScale(headStage);
        auto orientation = handStage.orientation;
        auto position = handStage.position - headStage.position;
        position = position * Environment::get().unitsPerMeter();

        auto camera = mViewer->getCamera();
        auto viewMatrix = camera->getViewMatrix();


        // Align orientation with the game world
        auto* inputManager = Environment::get().getInputManager();
        if (inputManager)
        {
            auto playerYaw = osg::Quat(-inputManager->mYaw, osg::Vec3d(0, 0, 1));
            position = playerYaw * position;
            orientation = orientation * playerYaw;
        }

        // Add camera offset
        osg::Vec3 viewPosition;
        osg::Vec3 center;
        osg::Vec3 up;

        viewMatrix.getLookAt(viewPosition, center, up, 1.0);
        position += viewPosition;

        //// Morrowind's meshes do not point forward by default.
        //// Static since they do not need to be recomputed.
        static float VRbias = osg::DegreesToRadians(-90.f);
        static osg::Quat yaw(VRbias, osg::Vec3f(0, 0, 1));
        static osg::Quat pitch(2.f * VRbias, osg::Vec3f(0, 1, 0));
        static osg::Quat roll (2.f * VRbias, osg::Vec3f(1, 0, 0));

        orientation = pitch * yaw * orientation;


        if (hand_transform->getName() == "tracker r hand")
            orientation = roll * orientation;
        
        // Hand are by default not well-centered
        // These numbers are just a rough guess
        osg::Vec3 offcenter = osg::Vec3(-0.175, 0., .033);
        if (hand_transform->getName() == "tracker r hand")
            offcenter.z() *= -1.;
        osg::Vec3 recenter = orientation * offcenter;
        position = position + recenter * Environment::get().unitsPerMeter();

        hand_transform->setAttitude(orientation);
        hand_transform->setPosition(position);
    }

    bool
        OpenXRViewer::TrackedNodeUpdateCallback::run(
            osg::Object* object, 
            osg::Object* data)
    {
        mViewer->updateTransformNode(object, data);
        return traverse(object, data);
    }
}
