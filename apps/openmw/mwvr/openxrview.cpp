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
        std::string name)
        : mXR(XR)
        , mSwapchain(nullptr)
        , mSwapchainConfig{}
        , mName(name)
    {
        mSwapchainConfig.requestedFormats = {
            GL_RGBA8,
            GL_RGBA8_SNORM,
        };

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
        camera->setRenderOrder(osg::Camera::PRE_RENDER, eye);
        camera->setComputeNearFarMode(osg::CullSettings::DO_NOT_COMPUTE_NEAR_FAR);
        camera->setAllowEventFocus(false);
        camera->setReferenceFrame(osg::Transform::ABSOLUTE_RF);
        camera->setViewport(0, 0, mSwapchain->width(), mSwapchain->height());
        camera->setGraphicsContext(gc);

        camera->setInitialDrawCallback(new OpenXRView::InitialDrawCallback());

        mPredraw = new OpenXRView::PredrawCallback(camera.get(), this);

        camera->setPreDrawCallback(mPredraw);
        camera->setFinalDrawCallback(new OpenXRView::PostdrawCallback(camera.get(), this));

        return camera.release();
    }

    void OpenXRView::setWidth(int width)
    {
        mSwapchainConfig.width = width;
    }

    void OpenXRView::setHeight(int height)
    {
        mSwapchainConfig.height = height;
    }

    void OpenXRView::setSamples(int samples)
    {
        mSwapchainConfig.samples = samples;
    }

    void OpenXRView::prerenderCallback(osg::RenderInfo& renderInfo)
    {
        Log(Debug::Verbose) << mName << ": prerenderCallback";
        if (mSwapchain)
        {
            mSwapchain->beginFrame(renderInfo.getState()->getGraphicsContext());
        }
    }

    void OpenXRView::postrenderCallback(osg::RenderInfo& renderInfo)
    {
        // osg will sometimes call this without a corresponding prerender.
        Log(Debug::Verbose) << mName << ": postrenderCallback";
        Log(Debug::Verbose) << renderInfo.getCurrentCamera()->getName() << ": " << renderInfo.getCurrentCamera()->getPreDrawCallback();
        if (renderInfo.getCurrentCamera()->getPreDrawCallback() != mPredraw)
        {
            // It seems OSG will sometimes overwrite the predraw callback.
            // Undocumented behaviour?
            renderInfo.getCurrentCamera()->setPreDrawCallback(mPredraw);
            Log(Debug::Warning) << ("osg overwrote predraw");
        }
        //if (mSwapchain)
        //    mSwapchain->endFrame(renderInfo.getState()->getGraphicsContext());

    }

    bool OpenXRView::realize(osg::ref_ptr<osg::State> state)
    {
        try {
            mSwapchain.reset(new OpenXRSwapchain(mXR, state, mSwapchainConfig));
        }
        catch (...)
        {
        }

        return !!mSwapchain;
    }

    void OpenXRView::PredrawCallback::operator()(osg::RenderInfo& info) const
    {
        mView->prerenderCallback(info);
    }

    void OpenXRView::PostdrawCallback::operator()(osg::RenderInfo& info) const
    {
        mView->postrenderCallback(info);
    }
}
