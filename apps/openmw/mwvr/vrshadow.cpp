#include "vrenvironment.hpp"
#include "vrsession.hpp"
#include "vrshadow.hpp"

#include "../mwrender/vismask.hpp"

#include <components/sceneutil/mwshadowtechnique.hpp>

#include <cassert>

namespace MWVR
{
    VrShadow::VrShadow(osgViewer::Viewer* viewer, int renderOrder)
        : mViewer(viewer)
        , mRenderOrder(renderOrder)
        , mShadowMapCamera(nullptr)
        , mUpdateCallback(new UpdateShadowMapSlaveCallback)
        , mMasterConfig(new SharedShadowMapConfig)
        , mSlaveConfig(new SharedShadowMapConfig)
    {
        mMasterConfig->_id = "VR";
        mMasterConfig->_master = true;
        mSlaveConfig->_id = "VR";
        mSlaveConfig->_master = false;
    }

    void VrShadow::configureShadowsForCamera(osg::Camera* camera)
    {
        camera->setUserData(mSlaveConfig);
        if (camera->getRenderOrderNum() < mRenderOrder)
            camera->setRenderOrder(camera->getRenderOrder(), mRenderOrder + 1);
    }

    void VrShadow::configureShadows(bool enabled)
    {
        if (enabled)
        {
            if (!mShadowMapCamera)
            {
                mShadowMapCamera = new osg::Camera();
                mShadowMapCamera->setName("ShadowMap");
                mShadowMapCamera->setClearMask(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
                mShadowMapCamera->setRenderTargetImplementation(osg::Camera::FRAME_BUFFER_OBJECT);
                mShadowMapCamera->setRenderOrder(osg::Camera::PRE_RENDER, mRenderOrder);
                mShadowMapCamera->setComputeNearFarMode(osg::CullSettings::DO_NOT_COMPUTE_NEAR_FAR);
                mShadowMapCamera->setAllowEventFocus(false);
                mShadowMapCamera->setReferenceFrame(osg::Transform::ABSOLUTE_RF);
                mShadowMapCamera->setViewport(0, 0, 640, 360);
                mShadowMapCamera->setGraphicsContext(mViewer->getCamera()->getGraphicsContext());
                mShadowMapCamera->setCullMask(0);
                mShadowMapCamera->setUserData(mMasterConfig);
                mViewer->addSlave(mShadowMapCamera, true);
                auto* slave = mViewer->findSlaveForCamera(mShadowMapCamera);
                assert(slave);
                slave->_updateSlaveCallback = mUpdateCallback;
            }

        }
        else
        {
            if (mShadowMapCamera)
            {
                mViewer->removeSlave(mViewer->findSlaveIndexForCamera(mShadowMapCamera));
                mShadowMapCamera = nullptr;
            }
        }
    }

    void UpdateShadowMapSlaveCallback::updateSlave(osg::View& view, osg::View::Slave& slave)
    {
        auto* camera = slave._camera.get();
        auto* session = Environment::get().getSession();
        auto viewMatrix = view.getCamera()->getViewMatrix();

        auto& poses = session->predictedPoses(VRSession::FramePhase::Update);
        auto& leftView = poses.view[(int)Side::LEFT_SIDE];
        auto& rightView = poses.view[(int)Side::RIGHT_SIDE];
        osg::Vec3d leftEye = leftView.pose.position;
        osg::Vec3d rightEye = rightView.pose.position;

        // The shadow map will be computed from a position P slightly behind the eyes L and R
        // where it creates the minimum frustum encompassing both eyes' frustums.

        // Compute Frustum angles. A simple min/max.
        FieldOfView fov;
        fov.angleLeft = std::min(leftView.fov.angleLeft, rightView.fov.angleLeft);
        fov.angleRight = std::max(leftView.fov.angleRight, rightView.fov.angleRight);
        fov.angleDown = std::min(leftView.fov.angleDown, rightView.fov.angleDown);
        fov.angleUp = std::max(leftView.fov.angleUp, rightView.fov.angleUp);

        // Use the law of sines on the triangle spanning PLR to determine P
        double angleLeft = std::abs(fov.angleLeft);
        double angleRight = std::abs(fov.angleRight);
        double lengthRL = (rightEye - leftEye).length();
        double ratioRL = lengthRL / std::sin(osg::PI - angleLeft - angleRight);
        double lengthLP = ratioRL * std::sin(angleRight);
        osg::Vec3d directionLP = osg::Vec3(std::cos(-angleLeft), std::sin(-angleLeft), 0);
        osg::Vec3d P = leftEye + directionLP * lengthLP;

        // Generate the matrices
        float near_ = Settings::Manager::getFloat("near clip", "Camera");
        float far_ = Settings::Manager::getFloat("viewing distance", "Camera");

        auto modifiedViewMatrix = viewMatrix * session->viewMatrix(P, osg::Quat(0, 0, 0, 1));
        auto projectionMatrix = fov.perspectiveMatrix(near_, far_);

        camera->setViewMatrix(modifiedViewMatrix);
        camera->setProjectionMatrix(projectionMatrix);
        slave.updateSlaveImplementation(view);
    }
}
