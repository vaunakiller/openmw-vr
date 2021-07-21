#include "vrtracking.hpp"
#include "vrenvironment.hpp"
#include "vrsession.hpp"
#include "openxrmanagerimpl.hpp"

#include <components/misc/constants.hpp>

#include <mutex>

namespace MWVR
{
    const VRTrackingView& VRView::locate(DisplayTime predictedDisplayTime)
    {
        if(predictedDisplayTime > mView.time)
        {
            VRTrackingView view = VRTrackingView();
            view.status = TrackingStatus::Good;
            view.time = predictedDisplayTime;
            locateImpl(predictedDisplayTime, view);
            if (!!view.status)
                mView = view;
        }
        
        return mView;
    }

    VRTrackingSource::VRTrackingSource(VRPath path)
        : mPath(path)
    {
        Environment::get().getTrackingManager()->registerTrackingSource(this);
    }

    VRTrackingSource::~VRTrackingSource()
    {
        Environment::get().getTrackingManager()->unregisterTrackingSource(this);
    }

    bool VRTrackingSource::availablePosesChanged() const
    {
        return mAvailablePosesChanged;
    }

    void VRTrackingSource::clearAvailablePosesChanged()
    {
        mAvailablePosesChanged = false;
    }

    void VRTrackingSource::notifyAvailablePosesChanged()
    {
        mAvailablePosesChanged = true;
    }

    VRTrackingListener::VRTrackingListener()
    {
        auto* tm = Environment::get().getTrackingManager();
        tm->addListener(this);
    }

    VRTrackingListener::~VRTrackingListener()
    {
        auto* tm = Environment::get().getTrackingManager();
        tm->removeListener(this);
    }

    VRTrackingManager::VRTrackingManager()
    {
        mHandDirectedMovement = Settings::Manager::getBool("hand directed movement", "VR");
        mHeadPath = stringToVRPath("/stage/user/head/input/pose");
        mHandPath = stringToVRPath("/stage/user/hand/left/input/aim/pose");
    }

    VRTrackingManager::~VRTrackingManager()
    {
    }

    void VRTrackingManager::addListener(VRTrackingListener* listener)
    {
        mListeners.push_back(listener);
    }

    void VRTrackingManager::removeListener(VRTrackingListener* listener)
    {
        for (auto it = mListeners.begin(); it != mListeners.end(); it++)
            if (*it == listener)
                it = mListeners.erase(it);
    }

    VRPath VRTrackingManager::stringToVRPath(const std::string& path)
    {
        // Empty path is invalid
        if (path.empty())
        {
            Log(Debug::Error) << "Empty path";
            return VRPath();
        }

        // Return path immediately if it already exists
        auto it = mPathIdentifiers.find(path);
        if (it != mPathIdentifiers.end())
            return it->second;

        // Add new path and return it
        return newPath(path);
    }

    std::string VRTrackingManager::VRPathToString(VRPath path)
    {
        // Find the identifier in the map and return the corresponding string.
        for (auto& e : mPathIdentifiers)
            if (e.second == path)
                return e.first;

        // No path found, return empty string
        Log(Debug::Warning) << "No such path identifier (" << path << ")";
        return "";
    }

    void VRTrackingManager::movementAngles(float& yaw, float& pitch)
    {
        yaw = mMovementYaw;
        pitch = mMovementPitch;
    }

    void VRTrackingManager::processChangedSettings(const std::set<std::pair<std::string, std::string>>& changed)
    {
        mHandDirectedMovement = Settings::Manager::getBool("hand directed movement", "VR");
    }

    VRTrackingPose VRTrackingManager::locate(VRPath path, DisplayTime predictedDisplayTime) const
    {
        auto it = mPathToSourceMap.find(path);
        if (it != mPathToSourceMap.end())
            return it->second->locate(path, predictedDisplayTime);

        VRTrackingPose pose = VRTrackingPose();
        pose.status = TrackingStatus::NotTracked;
        return pose;
    }

    VRTrackingSource* VRTrackingManager::getTrackingSource(VRPath path) const
    {
        for (auto* source : mSources)
            if (source->path() == path)
                return source;
        return nullptr;
    }

    const std::set<VRPath>& VRTrackingManager::availablePaths() const
    {
        return mAvailablePaths;
    }

    void VRTrackingManager::registerTrackingSource(VRTrackingSource* source)
    {
        mSources.emplace_back(source);
    }

    void VRTrackingManager::unregisterTrackingSource(VRTrackingSource* source)
    {
        for (auto it = mSources.begin(); it != mSources.end(); it++)
            if (*it == source)
                it = mSources.erase(it);
    }

    void VRTrackingManager::updateTracking()
    {
        // TODO: endFrame() call here should be moved into beginFrame()
        MWVR::Environment::get().getSession()->endFrame();
        MWVR::Environment::get().getSession()->beginFrame();
        auto& frame = Environment::get().getSession()->getFrame(VRSession::FramePhase::Update);
        auto predictedDisplayTime = frame->mFrameInfo.runtimePredictedDisplayTime;
        if (predictedDisplayTime == 0)
            return;

        checkAvailablePathsChanged();
        updateMovementAngles(predictedDisplayTime);

        for (auto* listener : mListeners)
            listener->onTrackingUpdated(*this, predictedDisplayTime);
    }

    void VRTrackingManager::updateMovementAngles(DisplayTime predictedDisplayTime)
    {
        if (mHandDirectedMovement)
        {
            auto tpHead = locate(mHeadPath, predictedDisplayTime);
            auto tpHand = locate(mHandPath, predictedDisplayTime);

            if (!!tpHead.status && !!tpHand.status)
            {
                float headYaw = 0.f;
                float headPitch = 0.f;
                float headsWillRoll = 0.f;

                float handYaw = 0.f;
                float handPitch = 0.f;
                float handRoll = 0.f;
                getEulerAngles(tpHead.pose.orientation, headYaw, headPitch, headsWillRoll);
                getEulerAngles(tpHand.pose.orientation, handYaw, handPitch, handRoll);

                mMovementYaw = handYaw - headYaw;
                mMovementPitch = handPitch - headPitch;
            }
        }
        else
        {
            mMovementYaw = 0;
            mMovementPitch = 0;
        }
    }

    void VRTrackingManager::checkAvailablePathsChanged()
    {
        bool availablePosesChanged = false;

        for (auto source : mSources)
            availablePosesChanged |= source->availablePosesChanged();

        if (availablePosesChanged)
            updateAvailablePaths();

        for (auto source : mSources)
            source->clearAvailablePosesChanged();
    }

    void VRTrackingManager::updateAvailablePaths()
    {
        mAvailablePaths.clear();
        mPathToSourceMap.clear();
        for (auto source : mSources)
        {
            auto paths = source->listSupportedPaths();
            mAvailablePaths.insert(paths.begin(), paths.end());
            for (auto& path : paths)
            {
                mPathToSourceMap.emplace(path, source);
            }
        }
        for (auto* listener : mListeners)
            listener->onAvailablePathsChanged(mAvailablePaths);
    }

    VRPath VRTrackingManager::newPath(const std::string& path)
    {
        VRPath vrPath = mPathIdentifiers.size() + 1;
        auto res = mPathIdentifiers.emplace(path, vrPath);
        return res.first->second;
    }

    VRStageToWorldBinding::VRStageToWorldBinding(VRPath path, std::shared_ptr<VRTrackingSource> source, VRPath movementReference)
        : VRTrackingSource(path)
        , mMovementReference(movementReference)
        , mSource(source)
    {
    }

    void VRStageToWorldBinding::setWorldOrientation(float yaw, bool adjust)
    {
        auto yawQuat = osg::Quat(yaw, osg::Vec3(0, 0, -1));
        if (adjust)
            mOrientation = yawQuat * mOrientation;
        else
            mOrientation = yawQuat;
    }

    void VRStageToWorldBinding::consumeMovement(const osg::Vec3& movement)
    {
        mMovement.x() -= movement.x();
        mMovement.y() -= movement.y();
    }

    void VRStageToWorldBinding::recenter(bool resetZ)
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

    void VRStageToWorldBinding::bindPaths(VRPath worldPath, VRPath stagePath)
    {
        mBindings.emplace(worldPath, stagePath);
        notifyAvailablePosesChanged();
    }

    void VRStageToWorldBinding::unbindPath(VRPath worldPath)
    {
        mBindings.erase(worldPath);
        notifyAvailablePosesChanged();
    }

    VRTrackingPose VRStageToWorldBinding::locate(VRPath path, DisplayTime predictedDisplayTime)
    {
        if (predictedDisplayTime > mLastPose.time)
            updateTracking(predictedDisplayTime);

        auto it = mBindings.find(path);
        if (it == mBindings.end())
        {
            Log(Debug::Error) << "Tried to locate invalid path " << path << ". This is a developer error. Please locate using TrackingManager::locate() only.";
            throw std::logic_error("Invalid Argument");
        }

        auto* tm = Environment::get().getTrackingManager();
        auto stagePose = tm->locate(it->second, predictedDisplayTime);

        auto worldPose = stagePose;
        worldPose.pose.position *= Constants::UnitsPerMeter;
        worldPose.pose.position -= mLastPose.pose.position;
        worldPose.pose.position = mOrientation * worldPose.pose.position;
        worldPose.pose.position += mMovement;
        worldPose.pose.orientation = worldPose.pose.orientation * mOrientation;

        if (mOrigin)
            worldPose.pose.position += mOriginWorldPose.position;
        return worldPose;

    }

    std::vector<VRPath> VRStageToWorldBinding::listSupportedPaths() const
    {
        std::vector<VRPath> paths;
        for (auto pose : mBindings)
            paths.emplace_back(pose.first);
        return paths;
    }

    void VRStageToWorldBinding::updateTracking(DisplayTime predictedDisplayTime)
    {
        mOriginWorldPose = Pose();
        if (mOrigin)
        {
            auto worldMatrix = osg::computeLocalToWorld(mOrigin->getParentalNodePaths()[0]);
            mOriginWorldPose.position = worldMatrix.getTrans();
            mOriginWorldPose.orientation = worldMatrix.getRotate();
        }
        auto* tm = Environment::get().getTrackingManager();
        auto mtp = tm->locate(mMovementReference, predictedDisplayTime);
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


