#include "vrcamera.hpp"
#include "vrgui.hpp"
#include "vrinputmanager.hpp"
#include "vrenvironment.hpp"
#include "vranimation.hpp"

#include <components/sceneutil/visitor.hpp>

#include <components/misc/constants.hpp>

#include "../mwbase/environment.hpp"
#include "../mwbase/world.hpp"
#include "../mwbase/windowmanager.hpp"

#include "../mwworld/player.hpp"
#include "../mwworld/class.hpp"

#include "../mwmechanics/movement.hpp"

#include <osg/Quat>

namespace MWVR
{

    VRCamera::VRCamera(osg::Camera* camera)
        : MWRender::Camera(camera)
        , mRoll(0.f)
    {
        mVanityAllowed = false;
        mFirstPersonView = true;

        auto* vrGuiManager = MWVR::Environment::get().getGUIManager();
        vrGuiManager->setCamera(camera);
    }

    VRCamera::~VRCamera()
    {
    }

    void VRCamera::setShouldTrackPlayerCharacter(bool track)
    {
        mShouldTrackPlayerCharacter = track;
    }

    void VRCamera::recenter()
    {
        if (!mHasTrackingData)
            return;

        // Move position of head to center of character 
        // Z should not be affected
        mHeadOffset.x() = 0;
        mHeadOffset.y() = 0;

        auto* session = Environment::get().getSession();

        if (mShouldResetZ)
        {
            if (session->seatedPlay())
            {
                // Adjust offset to place the current pose roughly at eye level
                mHeadOffset.z() = session->eyeLevel() * Constants::UnitsPerMeter;
            }
            else
            {
                mHeadOffset.z() = mHeadPose.position.z();
            }
            mShouldResetZ = false;
        }
        mShouldRecenter = false;
        Log(Debug::Verbose) << "Recentered";
    }

    void VRCamera::applyTracking()
    {
        MWBase::World* world = MWBase::Environment::get().getWorld();

        auto& player = world->getPlayer();
        auto playerPtr = player.getPlayer();
        
        osg::Quat orientation;
        getOrientation(orientation);

        float yaw = 0.f;
        float pitch = 0.f;
        float roll = 0.f;
        getEulerAngles(orientation, yaw, pitch, roll);

        if (!player.isDisabled() && mTrackingNode)
        {
            world->rotateObject(playerPtr, pitch, 0.f, yaw, MWBase::RotationFlag_none);
        }
    }

    void VRCamera::updateTracking()
    {
        auto* session = Environment::get().getSession();
        auto& frameMeta = session->getFrame(VRSession::FramePhase::Update);
        // Only update tracking if rendering.
        // OpenXR does not provide tracking information while not rendering.
        if (frameMeta && frameMeta->mShouldRender)
        {
            auto currentHeadPose = frameMeta->mPredictedPoses.head;
            currentHeadPose.position *= Constants::UnitsPerMeter;
            osg::Vec3 vrMovement = currentHeadPose.position - mHeadPose.position;
            mHeadPose = currentHeadPose;
            mHeadOffset += stageRotation() * vrMovement;
            mHasTrackingData = true;
        }
    }

    void VRCamera::updateCamera(osg::Camera* cam)
    {
        updateTracking();

        if (mShouldRecenter)
        {
            recenter();
            Camera::updateCamera(cam);
            auto* vrGuiManager = MWVR::Environment::get().getGUIManager();
            vrGuiManager->updateTracking();
        }
        else
        {
            if (mShouldTrackPlayerCharacter && !MWBase::Environment::get().getWindowManager()->isGuiMode())
                applyTracking();

            Camera::updateCamera(cam);
        }
    }

    void VRCamera::updateCamera()
    {
        Camera::updateCamera();
    }

    void VRCamera::reset()
    {
        Camera::reset();
    }

    void VRCamera::rotateCamera(float pitch, float roll, float yaw, bool adjust)
    {
        if (adjust)
        {
            pitch += getPitch();
            yaw += getYaw();
            roll += getRoll();
        }
        setYaw(yaw);
        setPitch(pitch);
        setRoll(roll);
    }

    void VRCamera::setRoll(float angle)
    {
        if (angle > osg::PI) {
            angle -= osg::PI * 2;
        }
        else if (angle < -osg::PI) {
            angle += osg::PI * 2;
        }
        mRoll = angle;
    }
    void VRCamera::toggleViewMode(bool force)
    {
        mFirstPersonView = true;
    }
    bool VRCamera::toggleVanityMode(bool enable)
    {
        // Vanity mode makes no sense in VR
        return Camera::toggleVanityMode(false);
    }
    void VRCamera::allowVanityMode(bool allow)
    {
        // Vanity mode makes no sense in VR
        mVanityAllowed = false;
    }
    void VRCamera::getPosition(osg::Vec3d& focal, osg::Vec3d& camera) const
    {
        Camera::getPosition(focal, camera);
        camera += mHeadOffset;
    }
    void VRCamera::getOrientation(osg::Quat& orientation) const
    {
        orientation = mHeadPose.orientation * osg::Quat(-mYawOffset, osg::Vec3d(0, 0, 1));
    }

    void VRCamera::processViewChange()
    {
        SceneUtil::FindByNameVisitor findRootVisitor("Player Root", osg::NodeVisitor::TRAVERSE_PARENTS);
        mAnimation->getObjectRoot()->accept(findRootVisitor);
        mTrackingNode = findRootVisitor.mFoundNode;

        if (!mTrackingNode)
            throw std::logic_error("Unable to find tracking node for VR camera");
        mHeightScale = 1.f;
    }

    void VRCamera::rotateCameraToTrackingPtr()
    {
        Camera::rotateCameraToTrackingPtr();
        setRoll(-mTrackingPtr.getRefData().getPosition().rot[1] - mDeferredRotation.y());

        // When the cell changes, openmw rotates the character.
        // To make sure the player faces the same direction regardless of current orientation,
        // compute the offset from character orientation to player orientation and reset yaw offset to this.
        float yaw = 0.f;
        float pitch = 0.f;
        float roll = 0.f;
        getEulerAngles(mHeadPose.orientation, yaw, pitch, roll);
        mYawOffset = -mYaw - yaw;
    }

    osg::Quat VRCamera::stageRotation()
    {
        return osg::Quat(mYawOffset, osg::Vec3(0, 0, -1));
    }

    void VRCamera::requestRecenter(bool resetZ)
    {
        mShouldRecenter = true;

        // Use OR so we don't negate a pending requests.
        mShouldResetZ |= resetZ;
    }
}
