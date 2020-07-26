#include "vrenvironment.hpp"
#include "vrsession.hpp"
#include "vrshadow.hpp"

#include "../mwrender/vismask.hpp"

#include <components/sceneutil/mwshadowtechnique.hpp>

#include <cassert>

namespace MWVR
{
    VrShadow::VrShadow()
        : mMasterConfig(new SharedShadowMapConfig)
        , mSlaveConfig(new SharedShadowMapConfig)
    {
        mMasterConfig->_id = "VR";
        mMasterConfig->_master = true;
        mSlaveConfig->_id = "VR";
        mSlaveConfig->_master = false;
    }

    void VrShadow::configureShadowsForCamera(osg::Camera* camera, bool master)
    {
        if(master)
            camera->setUserData(mMasterConfig);
        else
            camera->setUserData(mSlaveConfig);
    }

    void VrShadow::updateShadowConfig(osg::View& view)
    {
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

        if (mMasterConfig->_projection == nullptr)
            mMasterConfig->_projection = new osg::RefMatrix;
        if (mMasterConfig->_modelView == nullptr)
            mMasterConfig->_modelView = new osg::RefMatrix;
        mMasterConfig->_referenceFrame = view.getCamera()->getReferenceFrame();
        mMasterConfig->_modelView->set(modifiedViewMatrix);
        mMasterConfig->_projection->set(projectionMatrix);
    }
}
