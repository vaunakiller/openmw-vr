#include "trackingsource.hpp"
#include "trackingmanager.hpp"
#include "frame.hpp"
#include "session.hpp"

#include <components/debug/debuglog.hpp>
#include <components/misc/constants.hpp>

namespace VR
{
    TrackingSource::TrackingSource(VRPath path)
        : mPath(path)
    {
        TrackingManager::instance().registerTrackingSource(this);
    }

    TrackingSource::~TrackingSource()
    {
        TrackingManager::instance().unregisterTrackingSource(this);
    }

    bool TrackingSource::availablePosesChanged() const
    {
        return mAvailablePosesChanged;
    }

    void TrackingSource::clearAvailablePosesChanged()
    {
        mAvailablePosesChanged = false;
    }

    void TrackingSource::notifyAvailablePosesChanged()
    {
        mAvailablePosesChanged = true;
    }

    StageToWorldBinding::StageToWorldBinding(VRPath path, VRPath movementReference)
        : TrackingSource(path)
        , mMovementReference(movementReference)
    {
    }

    void StageToWorldBinding::setWorldOrientation(float yaw, bool adjust)
    {
        auto yawQuat = osg::Quat(yaw, osg::Vec3(0, 0, -1));
        if (adjust)
            mOrientation = yawQuat * mOrientation;
        else
            mOrientation = yawQuat;
    }

    void StageToWorldBinding::consumeMovement(const osg::Vec3& movement)
    {
        mMovement.x() -= movement.x();
        mMovement.y() -= movement.y();
    }

    void StageToWorldBinding::recenter(bool resetZ)
    {
        mMovement.x() = 0;
        mMovement.y() = 0;
        if (resetZ)
        {
            if (mSeatedPlay)
                mMovement.z() = mEyeLevel;
            else
                mMovement.z() = mLastPose.pose.position.z();
        }
    }

    void StageToWorldBinding::bindPaths(VRPath worldPath, VRPath stagePath)
    {
        mBindings.emplace(worldPath, stagePath);
        notifyAvailablePosesChanged();
    }

    void StageToWorldBinding::unbindPath(VRPath worldPath)
    {
        mBindings.erase(worldPath);
        notifyAvailablePosesChanged();
    }

    TrackingPose StageToWorldBinding::locate(VRPath path, DisplayTime predictedDisplayTime)
    {
        if (predictedDisplayTime != mLastPose.time)
            updateTracking(predictedDisplayTime);

        auto it = mBindings.find(path);
        if (it == mBindings.end())
        {
            Log(Debug::Error) << "Tried to locate invalid path " << path << ". This is a developer error. Please locate using TrackingManager::locate() only.";
            throw std::logic_error("Invalid Argument");
        }

        auto stagePose = TrackingManager::instance().locate(it->second, predictedDisplayTime);

        auto worldPose = stagePose;
        worldPose.pose.position *= Session::instance().playerScale();
        worldPose.pose.position *= Constants::UnitsPerMeter;
        worldPose.pose.position -= mLastPose.pose.position;
        worldPose.pose.position = mOrientation * worldPose.pose.position;
        worldPose.pose.position += mMovement;
        worldPose.pose.orientation = worldPose.pose.orientation * mOrientation;

        if (mOrigin)
            worldPose.pose.position += mOriginWorldPose.position;
        return worldPose;
    }

    std::vector<VRPath> StageToWorldBinding::listSupportedPaths() const
    {
        std::vector<VRPath> paths;
        for (auto pose : mBindings)
            paths.emplace_back(pose.first);
        return paths;
    }

    void StageToWorldBinding::updateTracking(VR::DisplayTime predictedDisplayTime)
    {
        mOriginWorldPose = Stereo::Pose();
        if (mOrigin)
        {
            auto worldMatrix = osg::computeLocalToWorld(mOrigin->getParentalNodePaths()[0]);
            mOriginWorldPose.position = worldMatrix.getTrans();
            mOriginWorldPose.orientation = worldMatrix.getRotate();
        }
        auto mtp = TrackingManager::instance().locate(mMovementReference, predictedDisplayTime);
        if (!!mtp.status)
        {
            mtp.pose.position *= Constants::UnitsPerMeter;
            osg::Vec3 vrMovement = mtp.pose.position - mLastPose.pose.position;
            mLastPose = mtp;
            if (mHasTrackingData)
                mMovement += mOrientation * vrMovement;
            else
                mMovement.z() = mLastPose.pose.position.z();
            mHasTrackingData = true;
        }
    }
}
