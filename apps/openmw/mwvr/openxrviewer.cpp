#include "openxrviewer.hpp"
#include "openxrmanagerimpl.hpp"
#include "Windows.h"

namespace MWVR
{

    OpenXRViewer::OpenXRViewer(
        osg::ref_ptr<OpenXRManager> XR,
        osg::ref_ptr<OpenXRManager::RealizeOperation> realizeOperation,
        osg::ref_ptr<osgViewer::Viewer> viewer,
        float metersPerUnit)
        : mXR(XR)
        , mRealizeOperation(realizeOperation)
        , mViewer(viewer)
        , mMetersPerUnit(metersPerUnit)
        , mConfigured(false)
    {

    }

    OpenXRViewer::~OpenXRViewer(void)
    {
    }

    void 
        OpenXRViewer::traverse(
            osg::NodeVisitor& visitor)
    {
        osg::ref_ptr<OpenXRManager::RealizeOperation> realizeOperation = nullptr;

        if (mRealizeOperation.lock(realizeOperation))
            if (mRealizeOperation->realized())
                if (configure())
                    osg::Group::traverse(visitor);
    }

    bool
        OpenXRViewer::configure()
    {
        if (mConfigured)
            return true;

        auto context = mViewer->getCamera()->getGraphicsContext();

        if (!context->makeCurrent())
        {
            Log(Debug::Warning) << "OpenXRViewer::configure() failed to make graphics context current.";
            return false;
        }
        
        context->setSwapCallback(new OpenXRManager::SwapBuffersCallback(mXR));

        auto DC = wglGetCurrentDC();
        auto GLRC = wglGetCurrentContext();

        if (DC != mXR->mPrivate->mGraphicsBinding.hDC)
        {
            Log(Debug::Warning) << "Graphics DC does not match DC used to construct XR context";
        }

        if (GLRC != mXR->mPrivate->mGraphicsBinding.hGLRC)
        {
            Log(Debug::Warning) << "Graphics GLRC does not match GLRC used to construct XR context";
        }

        osg::ref_ptr<osg::Camera> camera = mViewer->getCamera();
        camera->setName("Main");
        osg::Vec4 clearColor = camera->getClearColor();

        mViews[LEFT_VIEW] = new OpenXRView(mXR, context->getState(), mMetersPerUnit, 0);
        mViews[RIGHT_VIEW] = new OpenXRView(mXR, context->getState(), mMetersPerUnit, 1);

        osg::Camera* leftCamera = mViews[LEFT_VIEW]->createCamera(LEFT_VIEW, clearColor, context);
        osg::Camera* rightCamera = mViews[RIGHT_VIEW]->createCamera(RIGHT_VIEW, clearColor, context);

        leftCamera->setName("LeftEye");
        rightCamera->setName("RightEye");

        mViewer->addSlave(leftCamera, mViews[LEFT_VIEW]->projectionMatrix(), mViews[LEFT_VIEW]->viewMatrix(), true);
        mViewer->addSlave(rightCamera, mViews[RIGHT_VIEW]->projectionMatrix(), mViews[RIGHT_VIEW]->viewMatrix(), true);

        mViewer->getSlave(LEFT_VIEW)._updateSlaveCallback = new UpdateSlaveCallback(mXR, mViews[LEFT_VIEW]);
        mViewer->getSlave(RIGHT_VIEW)._updateSlaveCallback = new UpdateSlaveCallback(mXR, mViews[RIGHT_VIEW]);

        mViewer->setLightingMode(osg::View::SKY_LIGHT);
        mViewer->setReleaseContextAtEndOfFrameHint(false);

        // Rendering main camera is a waste of time with VR enabled
        //camera->setGraphicsContext(nullptr);
        mXRInput.reset(new OpenXRInputManager(mXR));

        mConfigured = true;
        return true;
    }

    void 
        OpenXRViewer::UpdateSlaveCallback::updateSlave(
            osg::View& view, 
            osg::View::Slave& slave)
    {
        mXR->handleEvents();
        if (!mXR->sessionRunning())
            return;

        auto* camera = slave._camera.get();
        auto name = camera->getName();
        Log(Debug::Debug) << "Updating camera " << name;

        if (camera->getName() == "LeftEye")
        {
            mXR->handleEvents();
            mXR->waitFrame();
            mXR->beginFrame();
            mXR->updateControls();
            mXR->updatePoses();
        }

        auto viewMatrix = view.getCamera()->getViewMatrix() * mView->viewMatrix();
        auto projMatrix = mView->projectionMatrix();

        camera->setViewMatrix(viewMatrix);
        camera->setProjectionMatrix(projMatrix);
        
    }
}
