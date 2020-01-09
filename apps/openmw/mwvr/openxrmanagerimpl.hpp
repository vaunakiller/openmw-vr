#ifndef OPENXR_MANAGER_IMPL_HPP
#define OPENXR_MANAGER_IMPL_HPP

#include "openxrmanager.hpp"
#include "../mwinput/inputmanagerimp.hpp"

#include <components/debug/debuglog.hpp>
#include <components/sdlutil/sdlgraphicswindow.hpp>

// The OpenXR SDK assumes we've included Windows.h
#include <Windows.h>

#include <openxr/openxr.h>
#include <openxr/openxr_platform.h>
#include <openxr/openxr_platform_defines.h>
#include <openxr/openxr_reflection.h>

#include <vector>
#include <array>
#include <map>
#include <iostream>

namespace MWVR
{

#define CHK_STRINGIFY(x) #x
#define TOSTRING(x) CHK_STRINGIFY(x)
#define FILE_AND_LINE __FILE__ ":" TOSTRING(__LINE__)
#define CHECK_XRCMD(cmd) CheckXrResult(cmd, #cmd, FILE_AND_LINE);
#define CHECK_XRRESULT(res, cmdStr) CheckXrResult(res, cmdStr, FILE_AND_LINE);

    XrResult CheckXrResult(XrResult res, const char* originator = nullptr, const char* sourceLocation = nullptr);

    struct OpenXRManagerImpl
    {

        OpenXRManagerImpl(void);
        ~OpenXRManagerImpl(void);

        void LogLayersAndExtensions();
        void LogInstanceInfo();
        void LogReferenceSpaces();

        const XrEventDataBaseHeader* nextEvent();
        void waitFrame();
        void beginFrame();
        void endFrame();
        std::array<XrView, 2> getStageViews();
        std::array<XrView, 2> getHmdViews();
        XrSpaceLocation getHeadLocation();
        int eyes();
        void handleEvents();
        void updateControls();
        void updatePoses();
        void setPoseUpdateCallback(OpenXRManager::PoseUpdateCallback::TrackedLimb limb, OpenXRManager::PoseUpdateCallback::TrackingMode mode, osg::ref_ptr<OpenXRManager::PoseUpdateCallback> cb);
        void HandleSessionStateChanged(const XrEventDataSessionStateChanged& stateChangedEvent);
        void setViewSubImage(int eye, const ::XrSwapchainSubImage& subImage);

        bool initialized = false;
        long long frameIndex = 0;
        XrInstance mInstance = XR_NULL_HANDLE;
        XrSession mSession = XR_NULL_HANDLE;
        XrSpace mSpace = XR_NULL_HANDLE;
        XrFormFactor mFormFactor = XR_FORM_FACTOR_HEAD_MOUNTED_DISPLAY;
        XrViewConfigurationType mViewConfigType = XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO;
        XrEnvironmentBlendMode mEnvironmentBlendMode = XR_ENVIRONMENT_BLEND_MODE_OPAQUE;
        XrSystemId mSystemId = XR_NULL_SYSTEM_ID;
        XrGraphicsBindingOpenGLWin32KHR mGraphicsBinding{ XR_TYPE_GRAPHICS_BINDING_OPENGL_WIN32_KHR };
        XrSystemProperties mSystemProperties{ XR_TYPE_SYSTEM_PROPERTIES };
        std::array<XrViewConfigurationView, 2> mConfigViews{ { {XR_TYPE_VIEW_CONFIGURATION_VIEW}, {XR_TYPE_VIEW_CONFIGURATION_VIEW} } };
        XrSpace mReferenceSpaceView = XR_NULL_HANDLE;
        XrSpace mReferenceSpaceStage = XR_NULL_HANDLE;
        XrEventDataBuffer mEventDataBuffer{ XR_TYPE_EVENT_DATA_BUFFER };
        XrCompositionLayerProjection mLayer{ XR_TYPE_COMPOSITION_LAYER_PROJECTION };
        XrCompositionLayerBaseHeader const* mLayer_p = reinterpret_cast<XrCompositionLayerBaseHeader*>(&mLayer);
        std::array<XrCompositionLayerProjectionView, 2> mProjectionLayerViews{ { {XR_TYPE_COMPOSITION_LAYER_PROJECTION_VIEW}, {XR_TYPE_COMPOSITION_LAYER_PROJECTION_VIEW} } };
        XrFrameState mFrameState{ XR_TYPE_FRAME_STATE };
        XrSessionState mSessionState = XR_SESSION_STATE_UNKNOWN;
        bool mSessionRunning = false;
        
        XrPosef mHeadTrackedPose{};
        XrPosef mLeftHandTrackedPose{};
        XrPosef mRightHandTrackedPose{};

        osg::ref_ptr<OpenXRManager::PoseUpdateCallback> mHeadAbsoluteCB = nullptr;
        osg::ref_ptr<OpenXRManager::PoseUpdateCallback> mHeadRelativeCB = nullptr;
        osg::ref_ptr<OpenXRManager::PoseUpdateCallback> mLeftHandAbsoluteCB = nullptr;
        osg::ref_ptr<OpenXRManager::PoseUpdateCallback> mLeftHandRelativeCB = nullptr;
        osg::ref_ptr<OpenXRManager::PoseUpdateCallback> mRightHandAbsoluteCB = nullptr;
        osg::ref_ptr<OpenXRManager::PoseUpdateCallback> mRightHandRelativeCB = nullptr;
    };
}

#endif 
