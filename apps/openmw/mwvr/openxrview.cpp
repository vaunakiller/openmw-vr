#include "openxrview.hpp"
#include "openxrmanager.hpp"
#include "openxrmanagerimpl.hpp"
#include "../mwinput/inputmanagerimp.hpp"

#include <components/debug/debuglog.hpp>
#include <components/sdlutil/sdlgraphicswindow.hpp>

#include <Windows.h>

#include <openxr/openxr.h>
#include <openxr/openxr_platform.h>
#include <openxr/openxr_platform_defines.h>
#include <openxr/openxr_reflection.h>

namespace MWVR {

    OpenXRView::OpenXRView(
        osg::ref_ptr<OpenXRManager> XR,
        std::string name,
        OpenXRSwapchain::Config config,
        osg::ref_ptr<osg::State> state)
        : mXR(XR)
        , mSwapchainConfig{ config }
        , mSwapchain(new OpenXRSwapchain(mXR, state, mSwapchainConfig))
        , mName(name)
        , mTimer(mName.c_str())
    {
    }

    OpenXRView::~OpenXRView()
    {
    }

    osg::Camera* OpenXRView::createCamera(int eye, const osg::Vec4& clearColor, osg::GraphicsContext* gc)
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

        camera->setInitialDrawCallback(new OpenXRView::InitialDrawCallback());

        return camera.release();
    }

    void OpenXRView::prerenderCallback(osg::RenderInfo& renderInfo)
    {
        if (mSwapchain)
        {
            mSwapchain->beginFrame(renderInfo.getState()->getGraphicsContext());
        }
        mTimer.checkpoint("Prerender");
        Log(Debug::Verbose) << "XRView: PreRender";
    }

    void OpenXRView::postrenderCallback(osg::RenderInfo& renderInfo)
    {
        //    mSwapchain->endFrame(renderInfo.getState()->getGraphicsContext());

        mTimer.checkpoint("Postrender");
        Log(Debug::Verbose) << "XRView: PostRender";
        auto state = renderInfo.getState();
        auto gl = osg::GLExtensions::Get(state->getContextID(), false);
    }

    void OpenXRView::swapBuffers(osg::GraphicsContext* gc)
    {
        swapchain().endFrame(gc);
    }

    void  OpenXRView::setPredictedPose(const Pose& pose) 
    { 
        mPredictedPose = pose; 
        //Log(Debug::Verbose) << mName << " predicted pose updated to " << pose;
    };
}
