#ifndef OPENXR_TRACKER_HPP
#define OPENXR_TRACKER_HPP

#include <openxr/openxr.h>
#include "vrtracking.hpp"

#include <map>
#include <array>

namespace MWVR
{
    class OpenXRView : VRView
    {
    public:
        OpenXRView(XrSession session, XrSpace reference);

    protected:
        void locateImpl(DisplayTime predictedDisplayTime, VRTrackingView& view) override;

    private:
        XrSession mSession;
        XrSpace mReference;
    };

    //! Serves as a C++ wrapper of openxr spaces, but also bridges stage coordinates and game coordinates.
    //! Supports the compulsory sets of paths.
    class OpenXRTracker : public VRTrackingSource
    {
    public:
        OpenXRTracker(VRPath path, XrSpace referenceSpace);
        ~OpenXRTracker();

        void addTrackingSpace(VRPath path, XrSpace space);
        void deleteTrackingSpace(VRPath path);

        std::vector<VRPath> listSupportedPaths() const override;

    protected:
        void updateTracking(DisplayTime predictedDisplayTime);
        VRTrackingPose locate(VRPath path, DisplayTime predictedDisplayTime) override;

    private:
        std::array<View, 2> locateViews(DisplayTime predictedDisplayTime, XrSpace reference);
        void update(VRTrackingPose& pose, XrSpace space, DisplayTime predictedDisplayTime);

        XrSpace mReferenceSpace;
        DisplayTime mLastUpdate = 0;

        std::map<VRPath, std::pair<XrSpace, VRTrackingPose> > mSpaces;
    };
}

#endif
