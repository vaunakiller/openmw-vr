#ifndef GAME_MWVR_VRCAMERA_H
#define GAME_MWVR_VRCAMERA_H

#include <string>

#include <osg/ref_ptr>
#include <osg/Vec3>
#include <osg/Vec3d>
#include <osg/Quat>

#include "../mwrender/camera.hpp"
#include <components/vr/trackinglistener.hpp>

#include "vrtypes.hpp"

namespace MWVR
{
    /// \brief VR camera control
    class VRCamera : public MWRender::Camera, public VR::TrackingListener
    {
    public:

        VRCamera(osg::Camera* camera);
        ~VRCamera() override;

        /// Update the view matrix of \a cam
         void updateCamera(osg::Camera* cam) override;

        /// Update the view matrix of the current camera
        void updateCamera() override;

        /// Reset to defaults
        void reset() override;

        /// Set where the camera is looking at. Uses Morrowind (euler) angles
        /// \param rot Rotation angles in radians
        void rotateCamera(float pitch, float roll, float yaw, bool adjust) override;

        void toggleViewMode(bool force = false) override;

        bool toggleVanityMode(bool enable) override;
        void allowVanityMode(bool allow) override;

        /// Stores focal and camera world positions in passed arguments
        void getPosition(osg::Vec3d& focal, osg::Vec3d& camera) const override;

        /// Store camera orientation in passed arguments
        void getOrientation(osg::Quat& orientation) const override;

        void processViewChange() override;

        void instantTransition() override;

        void requestRecenter(bool resetZ);

        void setShouldTrackPlayerCharacter(bool track);

    protected:
        void recenter();
        void applyTracking();

        void onTrackingUpdated(VR::TrackingManager& manager, VR::DisplayTime predictedDisplayTime) override;

    private:
        Misc::Pose mHeadPose{};
        bool mShouldRecenter{ true };
        bool mShouldResetZ{ true };
        bool mHasTrackingData{ false };
        bool mShouldTrackPlayerCharacter{ false };
    };
}

#endif
