#include "vrview.hpp"
#include "vrsession.hpp"
#include "openxrmanager.hpp"
#include "openxrmanagerimpl.hpp"
#include "../mwinput/inputmanagerimp.hpp"
#include "../mwbase/environment.hpp"
#include "../mwbase/world.hpp"
#include "../mwrender/renderingmanager.hpp"
#include "../mwrender/water.hpp"
#include "vrenvironment.hpp"


#include <components/debug/debuglog.hpp>
#include <components/sdlutil/sdlgraphicswindow.hpp>

#include <Windows.h>

#include <openxr/openxr.h>
#include <openxr/openxr_platform.h>
#include <openxr/openxr_platform_defines.h>
#include <openxr/openxr_reflection.h>

#include <osgViewer/Renderer>

namespace MWVR {

    VRView::VRView(
        std::string name,
        OpenXRSwapchain::Config config,
        osg::ref_ptr<osg::State> state)
        : mSwapchainConfig{ config }
        , mSwapchain(new OpenXRSwapchain(state, mSwapchainConfig))
        , mName(name)
        , mTimer(mName.c_str())
    {
    }

    VRView::~VRView()
    {
    }

    class CullCallback : public osg::NodeCallback
    {
        void operator()(osg::Node* node, osg::NodeVisitor* nv)
        {
            const auto& name = node->getName();
            if (name == "LeftEye")
                Environment::get().getSession()->beginPhase(VRSession::FramePhase::Cull);
            traverse(node, nv);
        }
    };

    osg::Camera* VRView::createCamera(int eye, const osg::Vec4& clearColor, osg::GraphicsContext* gc)
    {
        osg::ref_ptr<osg::Camera> camera = new osg::Camera();
        camera->setClearColor(clearColor);
        camera->setClearMask(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        camera->setRenderTargetImplementation(osg::Camera::FRAME_BUFFER_OBJECT);
        camera->setRenderOrder(osg::Camera::PRE_RENDER, eye + 2);
        camera->setComputeNearFarMode(osg::CullSettings::DO_NOT_COMPUTE_NEAR_FAR);
        camera->setAllowEventFocus(false);
        camera->setReferenceFrame(osg::Transform::ABSOLUTE_RF);
        camera->setViewport(0, 0, mSwapchain->width(), mSwapchain->height());
        camera->setGraphicsContext(gc);

        camera->setInitialDrawCallback(new VRView::InitialDrawCallback());
        camera->setCullCallback(new CullCallback);

        return camera.release();
    }

    void VRView::prerenderCallback(osg::RenderInfo& renderInfo)
    {
        if (mSwapchain)
        {
            mSwapchain->beginFrame(renderInfo.getState()->getGraphicsContext());
        }
        mTimer.checkpoint("Prerender");

    }

    void VRView::InitialDrawCallback::operator()(osg::RenderInfo& renderInfo) const
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
    void VRView::UpdateSlaveCallback::updateSlave(
            osg::View& view,
            osg::View::Slave& slave)
    {
        mView->mTimer.checkpoint("UpdateSlave");

        auto* camera = slave._camera.get();
        auto name = camera->getName();

        Side side = Side::RIGHT_HAND;
        if (name == "LeftEye")
            side = Side::LEFT_HAND;

        auto* session = Environment::get().getSession();
        auto viewMatrix = view.getCamera()->getViewMatrix();

        auto modifiedViewMatrix = viewMatrix * session->viewMatrix(VRSession::FramePhase::Update, side);
        auto projectionMatrix = session->projectionMatrix(VRSession::FramePhase::Update, side);

        camera->setViewMatrix(modifiedViewMatrix);
        camera->setProjectionMatrix(projectionMatrix);
        slave.updateSlaveImplementation(view);
    }

    void VRView::postrenderCallback(osg::RenderInfo& renderInfo)
    {
        auto name = renderInfo.getCurrentCamera()->getName();
        mTimer.checkpoint("Postrender");
    }

    void VRView::swapBuffers(osg::GraphicsContext* gc)
    {
        mSwapchain->endFrame(gc);
    }
}
