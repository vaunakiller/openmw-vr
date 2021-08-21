#include "openxrinput.hpp"
#include "openxrmanagerimpl.hpp"
#include "openxrplatform.hpp"
#include "openxrtracker.hpp"
#include "openxrtypeconversions.hpp"
#include "vrenvironment.hpp"
#include "vrinputmanager.hpp"
#include "vrsession.hpp"

#include <components/misc/constants.hpp>

namespace MWVR
{
    OpenXRView::OpenXRView(XrSession session, XrSpace reference)
        : mSession(session)
        , mReference(reference)
    {
        if (!session || !reference)
            throw std::logic_error("Invalid argument to OpenXRSpace::OpenXRSpace()");
    }

    void OpenXRView::locateImpl(DisplayTime predictedDisplayTime, VRTrackingView& view)
    {
        std::array<XrView, 2> xrViews{ {{XR_TYPE_VIEW}, {XR_TYPE_VIEW}} };
        XrViewState viewState{ XR_TYPE_VIEW_STATE };
        uint32_t viewCount = 2;

        XrViewLocateInfo viewLocateInfo{ XR_TYPE_VIEW_LOCATE_INFO };
        viewLocateInfo.viewConfigurationType = XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO;
        viewLocateInfo.displayTime = predictedDisplayTime;
        viewLocateInfo.space = mReference;

        auto* xr = Environment::get().getManager();
        auto res = xrLocateViews(xr->impl().xrSession(), &viewLocateInfo, &viewState, viewCount, &viewCount, xrViews.data());
        if (XR_FAILED(res))
        {
            // Call failed, exit.
            CHECK_XRRESULT(res, "xrLocateViews");
            if (res == XR_ERROR_TIME_INVALID)
                view.status = TrackingStatus::TimeInvalid;
            else
                view.status = TrackingStatus::RuntimeFailure;
            return;
        }

        std::array<View, 2> vrViews{};
        view.view[(int)Side::LEFT_SIDE].pose = fromXR(xrViews[(int)Side::LEFT_SIDE].pose);
        view.view[(int)Side::RIGHT_SIDE].pose = fromXR(xrViews[(int)Side::RIGHT_SIDE].pose);
        view.view[(int)Side::LEFT_SIDE].fov = fromXR(xrViews[(int)Side::LEFT_SIDE].fov);
        view.view[(int)Side::RIGHT_SIDE].fov = fromXR(xrViews[(int)Side::RIGHT_SIDE].fov);
    }

    OpenXRTracker::OpenXRTracker(VRPath path, XrSpace referenceSpace)
        : VRTrackingSource(path)
        , mReferenceSpace(referenceSpace)
    {

    }

    OpenXRTracker::~OpenXRTracker()
    {
    }

    void OpenXRTracker::addTrackingSpace(VRPath path, XrSpace space)
    {
        mSpaces.emplace(path, std::pair(space, VRTrackingPose()));
    }

    void OpenXRTracker::deleteTrackingSpace(VRPath path)
    {
        mSpaces.erase(path);
    }

    std::vector<VRPath> OpenXRTracker::listSupportedPaths() const
    {
        std::vector<VRPath> paths;
        for (auto space : mSpaces)
            paths.emplace_back(space.first);
        return paths;
    }

    void OpenXRTracker::updateTracking(const VR::Frame& frame)
    {
        mLastUpdate = frame.predictedDisplayTime;
        if(frame.shouldSyncInput)
            Environment::get().getInputManager()->xrInput().getActionSet(ActionSet::Tracking).updateControls();

        for (auto& space : mSpaces)
        {
            update(space.second.second, space.second.first, frame.predictedDisplayTime);
        }
    }

    VRTrackingPose OpenXRTracker::locate(VRPath path, DisplayTime predictedDisplayTime)
    {
        if (predictedDisplayTime != mLastUpdate)
            throw std::logic_error("Locate called out of order");

        auto it = mSpaces.find(path);
        if (it != mSpaces.end())
            return it->second.second;

        Log(Debug::Error) << "Tried to locate invalid path " << path << ". This is a developer error. Please locate using TrackingManager::locate() only.";
        throw std::logic_error("Invalid Argument");
    }

    std::array<View, 2> OpenXRTracker::locateViews(DisplayTime predictedDisplayTime, XrSpace reference)
    {
        std::array<XrView, 2> xrViews{ {{XR_TYPE_VIEW}, {XR_TYPE_VIEW}} };
        XrViewState viewState{ XR_TYPE_VIEW_STATE };
        uint32_t viewCount = 2;

        XrViewLocateInfo viewLocateInfo{ XR_TYPE_VIEW_LOCATE_INFO };
        viewLocateInfo.viewConfigurationType = XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO;
        viewLocateInfo.displayTime = predictedDisplayTime;
        viewLocateInfo.space = reference;

        auto* xr = Environment::get().getManager();
        CHECK_XRCMD(xrLocateViews(xr->impl().xrSession(), &viewLocateInfo, &viewState, viewCount, &viewCount, xrViews.data()));

        std::array<View, 2> vrViews{};
        vrViews[(int)Side::LEFT_SIDE].pose = fromXR(xrViews[(int)Side::LEFT_SIDE].pose);
        vrViews[(int)Side::RIGHT_SIDE].pose = fromXR(xrViews[(int)Side::RIGHT_SIDE].pose);
        vrViews[(int)Side::LEFT_SIDE].fov = fromXR(xrViews[(int)Side::LEFT_SIDE].fov);
        vrViews[(int)Side::RIGHT_SIDE].fov = fromXR(xrViews[(int)Side::RIGHT_SIDE].fov);
        return vrViews;
    }

    void OpenXRTracker::update(VRTrackingPose& pose, XrSpace space, DisplayTime predictedDisplayTime)
    {
        pose.status = TrackingStatus::Good;
        pose.time = predictedDisplayTime;
        XrSpaceLocation location{ XR_TYPE_SPACE_LOCATION };
        auto res = xrLocateSpace(space, mReferenceSpace, predictedDisplayTime, &location);

        if (XR_FAILED(res))
        {
            // Call failed, exit.
            CHECK_XRRESULT(res, "xrLocateSpace");
            pose.status = TrackingStatus::RuntimeFailure;
            return;
        }

        // Check that everything is being tracked
        if (!(location.locationFlags & (XR_SPACE_LOCATION_ORIENTATION_TRACKED_BIT | XR_SPACE_LOCATION_POSITION_TRACKED_BIT)))
        {
            // It's not, data is stale
            pose.status = TrackingStatus::Stale;
        }

        // Check that data is valid
        if (!(location.locationFlags & (XR_SPACE_LOCATION_ORIENTATION_VALID_BIT | XR_SPACE_LOCATION_POSITION_VALID_BIT)))
        {
            // It's not, we've lost tracking
            pose.status = TrackingStatus::Lost;
            return;
        }

        pose.pose = MWVR::Pose{
        fromXR(location.pose.position),
        fromXR(location.pose.orientation)
        };
    }

}
