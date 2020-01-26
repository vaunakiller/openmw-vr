#include "openxrworldview.hpp"
#include "openxrmanager.hpp"
#include "openxrmanagerimpl.hpp"
#include "../mwinput/inputmanagerimp.hpp"

#include <components/debug/debuglog.hpp>
#include <components/sdlutil/sdlgraphicswindow.hpp>

#include <Windows.h>

#include <openxr/openxr.h>


#include <osg/Camera>
#include <osgViewer/Renderer>

#include <vector>
#include <array>
#include <iostream>

namespace MWVR
{
    // Some headers like to define these.
#ifdef near
#undef near
#endif

#ifdef far
#undef far
#endif

    static osg::Matrix
        perspectiveFovMatrix(float near, float far, XrFovf fov)
    {
        const float tanLeft = tanf(fov.angleLeft);
        const float tanRight = tanf(fov.angleRight);
        const float tanDown = tanf(fov.angleDown);
        const float tanUp = tanf(fov.angleUp);

        const float tanWidth = tanRight - tanLeft;
        const float tanHeight = tanUp - tanDown;

        const float offset = near;

        float matrix[16] = {};

        matrix[0] = 2 / tanWidth;
        matrix[4] = 0;
        matrix[8] = (tanRight + tanLeft) / tanWidth;
        matrix[12] = 0;

        matrix[1] = 0;
        matrix[5] = 2 / tanHeight;
        matrix[9] = (tanUp + tanDown) / tanHeight;
        matrix[13] = 0;

        if (far <= near) {
            matrix[2] = 0;
            matrix[6] = 0;
            matrix[10] = -1;
            matrix[14] = -(near + offset);
        }
        else {
            matrix[2] = 0;
            matrix[6] = 0;
            matrix[10] = -(far + offset) / (far - near);
            matrix[14] = -(far * (near + offset)) / (far - near);
        }

        matrix[3] = 0;
        matrix[7] = 0;
        matrix[11] = -1;
        matrix[15] = 0;

        return osg::Matrix(matrix);
    }


    osg::Matrix OpenXRWorldView::projectionMatrix()
    {
        auto hmdViews = mXR->impl().getPredictedViews(OpenXRFrameIndexer::instance().updateIndex(), TrackedSpace::VIEW);

        float near = Settings::Manager::getFloat("near clip", "Camera");
        float far = Settings::Manager::getFloat("viewing distance", "Camera") * mMetersPerUnit;
        //return perspectiveFovMatrix()
        return perspectiveFovMatrix(near, far, hmdViews[mView].fov);
    }

    osg::Matrix OpenXRWorldView::viewMatrix()
    {
        osg::Matrix viewMatrix;
        auto hmdViews = mXR->impl().getPredictedViews(OpenXRFrameIndexer::instance().updateIndex(), TrackedSpace::VIEW);
        auto pose = hmdViews[mView].pose;
        osg::Vec3 position = osg::fromXR(pose.position);

        auto stageViews = mXR->impl().getPredictedViews(OpenXRFrameIndexer::instance().updateIndex(), TrackedSpace::STAGE);
        auto stagePose = stageViews[mView].pose;

        // Comfort shortcut.
        // TODO: STAGE movement should affect in-game movement but not like this.
        // This method should only be using HEAD view.
        // But for comfort i'm keeping this until such movement has been implemented.
#if 1
        position = -osg::fromXR(stagePose.position);
        position.y() += 0.9144 * 2.;
#endif

        // invert orientation (conjugate of Quaternion) and position to apply to the view matrix as offset
        viewMatrix.setTrans(position * mMetersPerUnit);
        viewMatrix.postMultRotate(osg::fromXR(stagePose.orientation).conj());

        // Scale to world units
        //viewMatrix.postMultScale(osg::Vec3d(1.f / mMetersPerUnit, 1.f / mMetersPerUnit, 1.f / mMetersPerUnit));

        return viewMatrix;
    }

    OpenXRWorldView::OpenXRWorldView(
        osg::ref_ptr<OpenXRManager> XR, std::string name, osg::ref_ptr<osg::State> state, float metersPerUnit, SubView view)
        : OpenXRView(XR, name)
        , mMetersPerUnit(metersPerUnit)
        , mView(view)
    {
        auto config = mXR->impl().mConfigViews[view];

        setWidth(config.recommendedImageRectWidth);
        setHeight(config.recommendedImageRectHeight);
        setSamples(config.recommendedSwapchainSampleCount);

        realize(state);
        //    XR->setViewSubImage(view, mSwapchain->subImage());
    }

    OpenXRWorldView::~OpenXRWorldView()
    {

    }

    void OpenXRWorldView::InitialDrawCallback::operator()(osg::RenderInfo& renderInfo) const
    {
        osg::GraphicsOperation* graphicsOperation = renderInfo.getCurrentCamera()->getRenderer();
        osgViewer::Renderer* renderer = dynamic_cast<osgViewer::Renderer*>(graphicsOperation);
        if (renderer != nullptr)
        {
            // Disable normal OSG FBO camera setup because it will undo the MSAA FBO configuration.
            renderer->setCameraRequiresSetUp(false);
        }
    }

    void OpenXRWorldView::prerenderCallback(osg::RenderInfo& renderInfo)
    {
        OpenXRView::prerenderCallback(renderInfo);
        auto* view = renderInfo.getView();
        auto* camera = renderInfo.getCurrentCamera();
        auto name = camera->getName();

        Log(Debug::Verbose) << "Updating camera " << name;

        auto viewMatrix = view->getCamera()->getViewMatrix() * this->viewMatrix();
        auto projectionMatrix = this->projectionMatrix();

        camera->setViewMatrix(viewMatrix);
        camera->setProjectionMatrix(projectionMatrix);
    }

    void
        OpenXRWorldView::UpdateSlaveCallback::updateSlave(
            osg::View& view,
            osg::View::Slave& slave)
    {
        mXR->handleEvents();
        if (!mXR->sessionRunning())
            return;


        auto* camera = slave._camera.get();
        auto name = camera->getName();

        Log(Debug::Verbose) << "Updating slave " << name;

        //auto viewMatrix = view.getCamera()->getViewMatrix() * mView->viewMatrix();
        //auto projMatrix = mView->projectionMatrix();

        //camera->setViewMatrix(viewMatrix);
        //camera->setProjectionMatrix(projMatrix);

        slave.updateSlaveImplementation(view);
    }
}
