#ifndef MWVR_VRTYPES_H
#define MWVR_VRTYPES_H

#include <memory>
#include <array>
#include <mutex>
#include <components/debug/debuglog.hpp>
#include <components/sdlutil/sdlgraphicswindow.hpp>
#include <components/settings/settings.hpp>
#include <osg/Camera>
#include <osgViewer/Viewer>

namespace MWVR
{
    class OpenXRSwapchain;
    class OpenXRManager;

    //! Describes what limb to track.
    enum class TrackedLimb
    {
        LEFT_HAND,
        RIGHT_HAND,
        HEAD
    };

    //! Describes what space to track the limb in
    enum class ReferenceSpace
    {
        STAGE = 0, //!< Track limb in the VR stage space. Meaning a space with a floor level origin and fixed horizontal orientation.
        VIEW = 1 //!< Track limb in the VR view space. Meaning a space with the head as origin and orientation.
    };

    //! Self-descriptive
    enum class Side
    {
        LEFT_SIDE = 0,
        RIGHT_SIDE = 1
    };

    //! Represents the relative pose in space of some limb or eye.
    struct Pose
    {
        //! Position in space
        osg::Vec3 position{ 0,0,0 };
        //! Orientation in space.
        osg::Quat orientation{ 0,0,0,1 };

        //! Add one pose to another
        Pose operator+(const Pose& rhs);
        const Pose& operator+=(const Pose& rhs);

        //! Scale a pose (does not affect orientation)
        Pose operator*(float scalar);
        const Pose& operator*=(float scalar);
        Pose operator/(float scalar);
        const Pose& operator/=(float scalar);

        bool operator==(const Pose& rhs) const;
    };

    //! Fov of a single eye
    struct FieldOfView {
        float    angleLeft;
        float    angleRight;
        float    angleUp;
        float    angleDown;

        bool operator==(const FieldOfView& rhs) const;

        //! Generate a perspective matrix from this fov
        osg::Matrix perspectiveMatrix(float near, float far);
    };

    //! Represents an eye in VR including both pose and fov. A view's pose is relative to the head.
    struct View
    {
        Pose pose;
        FieldOfView fov;
        bool operator==(const View& rhs) const;
    };

    //! The complete set of poses tracked each frame by MWVR.
    struct PoseSet
    {
        Pose eye[2]{};   //!< Stage-relative
        Pose hands[2]{}; //!< Stage-relative
        Pose head{};     //!< Stage-relative
        View view[2]{};  //!< Head-relative

        bool operator==(const PoseSet& rhs) const;
    };

    struct CompositionLayerProjectionView
    {
        class OpenXRSwapchain* swapchain;
        Pose pose;
        FieldOfView fov;
    };

    struct SwapchainConfig
    {
        uint32_t recommendedWidth = -1;
        uint32_t maxWidth = -1;
        uint32_t recommendedHeight = -1;
        uint32_t maxHeight = -1;
        uint32_t recommendedSamples = -1;
        uint32_t maxSamples = -1;
    };

    struct FrameInfo
    {
        long long   runtimePredictedDisplayTime;
        long long   runtimePredictedDisplayPeriod;
        bool        runtimeRequestsRender;
    };

    // Serialization methods for VR types.
    std::ostream& operator <<(std::ostream& os, const Pose& pose);
    std::ostream& operator <<(std::ostream& os, const FieldOfView& fov);
    std::ostream& operator <<(std::ostream& os, const View& view);
    std::ostream& operator <<(std::ostream& os, const PoseSet& poseSet);
    std::ostream& operator <<(std::ostream& os, TrackedLimb limb);
    std::ostream& operator <<(std::ostream& os, ReferenceSpace space);
    std::ostream& operator <<(std::ostream& os, Side side);
}

#endif
