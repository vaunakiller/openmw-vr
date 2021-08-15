#ifndef MWVR_VRTRACKING_H
#define MWVR_VRTRACKING_H

#include <memory>
#include <map>
#include <set>
#include <vector>
#include <mutex>
#include <list>
#include "vrtypes.hpp"

namespace MWVR
{
    class VRAnimation;

    //! Describes the status of the tracking data. Note that there are multiple success statuses, and predicted poses should be used whenever the status is a non-negative integer.
    enum class TrackingStatus : signed
    {
        Unknown = 0, //!< No data has been written (default value)
        Good = 1,  //!< Accurate, up-to-date tracking data was used.
        Stale = 2, //!< Inaccurate, stale tracking data was used. This code is a status warning, not an error, and the tracking pose should be used.
        NotTracked = -1, //!< No tracking data was returned because no tracking source provides this path
        TimeInvalid = -2, //!< No tracking data was returned because the provided time stamp was invalid
        Lost = -3,  //!< No tracking data was returned because the tracking source could not be read (occluded controller, network connectivity issues, etc.).
        RuntimeFailure = -4 //!< No tracking data was returned because of a runtime failure.
    };

    inline bool operator!(TrackingStatus status)
    {
        return static_cast<signed>(status) < static_cast<signed>(TrackingStatus::Good);
    }

    //! @brief An identifier representing an path, with 0 representing no path.
    //! A VRPath can be optained from the string representation of a path using VRTrackingManager::stringToVRPath()
    //! \note Support is determined by each VRTrackingSource. ALL strings are convertible to VRPaths but won't be useful unless they match a supported string.
    //! \sa VRTrackingManager::getTrackingPath()
    using VRPath = uint32_t;

    //! A single tracked pose
    struct VRTrackingPose
    {
        TrackingStatus status = TrackingStatus::Unknown; //!< State of the prediction. 
        Pose pose = {}; //!< The predicted pose. 
        DisplayTime time = 0; //!< The time for which the pose was predicted.
    };

    //! A stereo view
    struct VRTrackingView
    {
        TrackingStatus status = TrackingStatus::Unknown; //!< State of the prediction. 
        std::array<View, 2> view = {}; //!< The predicted view. 
        DisplayTime time = 0; //!< The time for which the pose was predicted.
    };

    class VRView
    {
    public:
        VRView() {};
        virtual ~VRView() {};

        const VRTrackingView& locate(DisplayTime predictedDisplayTime);

    protected:
        virtual void locateImpl(DisplayTime predictedDisplayTime, VRTrackingView& view) = 0;

    private:
        VRTrackingView mView;
    };

    //! A class that acts as locating source for a unique set of VRPath identifiers
    //! A source is itself identified by a VRPath identifier
    class VRTrackingSource
    {
    public:
        VRTrackingSource(VRPath sourcePath);
        virtual ~VRTrackingSource();

        //! @brief Predicted pose of the given path at the predicted time
        //! 
        //! \arg predictedDisplayTime[in] Time to predict. This is normally the predicted display time. If time is 0, the last pose that was predicted is returned.
        //! \arg path[in] path of the pose requested. Should match an available pose path. 
        //! \arg reference[in] path of the pose to use as reference. If 0, pose is referenced to the VR stage.
        //! 
        //! \return A structure describing a pose and the tracking status.
        virtual VRTrackingPose locate(VRPath path, DisplayTime predictedDisplayTime) = 0;

        //! List currently supported tracking paths.
        virtual std::vector<VRPath> listSupportedPaths() const = 0;

        //! Returns true if the available poses changed since the last frame, false otherwise.
        bool availablePosesChanged() const;

        void clearAvailablePosesChanged();

        VRPath path() const { return mPath; }

    protected:

        void notifyAvailablePosesChanged();

        bool mAvailablePosesChanged = true;
        VRPath mPath = 0;
    };

    //! Ties a tracking source to the game world.
    //! A reference pose is selected by passing its path to the constructor. 
    //! All poses are transformed in the horizontal plane by moving the x,y origin to the position of the reference pose, and then reoriented using the current orientation of the player character.
    //! The reference pose is effectively always at the x,y origin, and its movement is accumulated and can be read using the movement() call.
    //! If this movement is ever consumed (such as by moving the character to follow the player) the consumed movement must be reported using consumeMovement().
    class VRStageToWorldBinding : public VRTrackingSource
    {
    public:
        VRStageToWorldBinding(VRPath sourcePath, std::shared_ptr<VRTrackingSource> source, VRPath movementReference);

        void setWorldOrientation(float yaw, bool adjust);
        osg::Quat getWorldOrientation() const { return mOrientation; }

        void setEyeLevel(float eyeLevel) { mEyeLevel = eyeLevel; }
        float getEyeLevel() const { return mEyeLevel; }

        void setSeatedPlay(bool seatedPlay) { mSeatedPlay = seatedPlay; }
        bool getSeatedPlay() const { return mSeatedPlay; }

        //! The player's movement within the VR stage. This accumulates until the movement has been consumed by calling consumeMovement()
        osg::Vec3 movement() const { return mMovement; }

        //! Consume movement
        void consumeMovement(const osg::Vec3& movement);

        //! Recenter tracking by consuming all movement.
        void recenter(bool resetZ);

        //! World origin is the point that ties the stage and the world. (0,0,0 in the world-aligned stage is this node).
        //! If no node is set, the world-aligned stage and the world correspond 1-1.
        void setOriginNode(osg::Node* origin) { mOrigin = origin; }

        void bindPaths(VRPath worldPath, VRPath stagePath);
        void unbindPath(VRPath worldPath);

    protected:
        //! Fetches a pose from the source, and then aligns it with the game world if the reference is 0 (stage). 
        VRTrackingPose locate(VRPath path, DisplayTime predictedDisplayTime) override;

        //! List currently supported tracking paths.
        std::vector<VRPath> listSupportedPaths() const override;

        //! Call once per frame, after (or at the end of) OSG update traversals and before cull traversals.
        //! Predict tracked poses for the given display time.
        //! \arg predictedDisplayTime [in] the predicted display time. The pose shall be predicted for this time based on current tracking data.
        void updateTracking(DisplayTime predictedDisplayTime);

    private:
        std::shared_ptr<VRTrackingSource> mSource;
        VRPath mMovementReference;
        std::map<VRPath, VRPath> mBindings;
        osg::Node* mOrigin = nullptr;
        bool mSeatedPlay = false;
        bool mHasTrackingData = false;
        float mEyeLevel = 0;
        Pose mOriginWorldPose = Pose();
        VRTrackingPose mLastPose = VRTrackingPose();
        osg::Vec3 mMovement = osg::Vec3(0, 0, 0);
        osg::Quat mOrientation = osg::Quat(0, 0, 0, 1);
    };

    class VRTrackingManager;

    class VRTrackingListener
    {
    public:
        VRTrackingListener();
        virtual ~VRTrackingListener();

        //! Notify that available tracking poses have changed.
        virtual void onAvailablePathsChanged(const std::set<VRPath>& paths) {};

        //! Called every frame, after tracking poses have been updated
        virtual void onTrackingUpdated(VRTrackingManager& manager, DisplayTime predictedDisplayTime) = 0;

    private:
    };

    class VRTrackingManager
    {
    public:
        VRTrackingManager();
        ~VRTrackingManager();

        //! Angles to be used for overriding movement direction
        //void movementAngles(float& yaw, float& pitch);

        void updateTracking(DisplayTime predictedDisplayTime);

        //! Bind listener to source, listener will receive tracking updates from source until unbound.
        //! \note A single listener can only receive tracking updates from one source.
        void addListener(VRTrackingListener* listener);

        //! Unbind listener, listener will no longer receive tracking updates.
        void removeListener(VRTrackingListener* listener);

        //! Converts a string representation of a path to a VRTrackerPath identifier
        VRPath stringToVRPath(const std::string& path);

        //! Converts a path identifier back to string. Returns an empty string if no such identifier exists.
        std::string VRPathToString(VRPath path);

        //! Angles to be used for overriding movement direction
        void movementAngles(float& yaw, float& pitch);

        void processChangedSettings(const std::set< std::pair<std::string, std::string> >& changed);

        VRTrackingPose locate(VRPath path, DisplayTime predictedDisplayTime) const;

        VRTrackingSource* getTrackingSource(VRPath path) const;

        const std::set<VRPath>& availablePaths() const;

    private:
        friend class VRTrackingSource;
        void registerTrackingSource(VRTrackingSource* source);
        void unregisterTrackingSource(VRTrackingSource* source);
        void updateMovementAngles(DisplayTime predictedDisplayTime);
        void checkAvailablePathsChanged();
        void updateAvailablePaths();
        VRPath newPath(const std::string& path);

        std::vector<VRTrackingSource*> mSources;
        std::map<VRPath, VRTrackingSource*> mPathToSourceMap;
        std::set<VRPath> mAvailablePaths;
        std::list<VRTrackingListener*> mListeners;
        std::map<std::string, VRPath> mPathIdentifiers;

        bool mHandDirectedMovement = 0.f;
        VRPath mHeadPath = VRPath();
        VRPath mHandPath = VRPath();
        float mMovementYaw = 0.f;
        float mMovementPitch = 0.f;
    };
}

#endif
