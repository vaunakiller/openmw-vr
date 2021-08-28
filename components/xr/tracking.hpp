#ifndef OPENXR_TRACKER_HPP
#define OPENXR_TRACKER_HPP

#include <openxr/openxr.h>
#include <components/vr/trackingsource.hpp>
#include <components/misc/stereo.hpp>

#include <map>
#include <array>

namespace XR
{
    //class OpenXRView : VRView
    //{
    //public:
    //    OpenXRView(XrSession session, XrSpace reference);

    //protected:
    //    void locateImpl(DisplayTime predictedDisplayTime, VRTrackingView& view) override;

    //private:
    //    XrSession mSession;
    //    XrSpace mReference;
    //};

    //! Serves as a C++ wrapper of openxr spaces
    //! Provides tracking of the following paths:
    //!     /stage/user/head/input/pose
    //!     /stage/user/hand/left/input/aim/pose
    //!     /stage/user/hand/left/input/aim/pose
    //! 
    class Tracker : public VR::TrackingSource
    {
    public:
        Tracker(VR::VRPath path, XrSpace referenceSpace);
        ~Tracker();

        void addTrackingSpace(VR::VRPath path, XrSpace space);
        void deleteTrackingSpace(VR::VRPath path);

        std::vector<VR::VRPath> listSupportedPaths() const override;

    protected:
        void updateTracking(VR::DisplayTime predictedDisplayTime) override;
        VR::TrackingPose locate(VR::VRPath path, VR::DisplayTime predictedDisplayTime) override;

    private:
        std::array<Misc::View, 2> locateViews(VR::DisplayTime predictedDisplayTime, XrSpace reference, XrSession session);
        void update(VR::TrackingPose& pose, XrSpace space, VR::DisplayTime predictedDisplayTime);

        XrSpace mReferenceSpace;
        VR::DisplayTime mLastUpdate = 0;

        std::map<VR::VRPath, std::pair<XrSpace, VR::TrackingPose> > mSpaces;
    };
}

#endif
