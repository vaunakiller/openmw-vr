#ifndef XR_INSTANCE_HPP
#define XR_INSTANCE_HPP

#include "platform.hpp"
#include "tracking.hpp"

#include <components/debug/debuglog.hpp>
#include <components/sdlutil/sdlgraphicswindow.hpp>
#include <components/misc/stereo.hpp>
#include <components/vr/directx.hpp>
#include <components/vr/directx.hpp>
#include <components/vr/constants.hpp>

#include <openxr/openxr.h>

#include <vector>
#include <array>
#include <map>
#include <iostream>
#include <thread>
#include <chrono>
#include <queue>

namespace VR
{
    class Session;
}

namespace XR
{

    struct SwapchainConfig
    {
        int recommendedWidth = -1;
        int recommendedHeight = -1;
        int recommendedSamples = -1;
        int maxWidth = -1;
        int maxHeight = -1;
        int maxSamples = -1;
    };

    /// \brief Instantiates and manages and openxr instance.
    class Instance
    {
    public:
        static Instance& instance();

    public:
        Instance(osg::GraphicsContext* gc);
        ~Instance(void);

        void endFrame(VR::Frame& frame);
        std::array<Misc::View, 2> getPredictedViews(int64_t predictedDisplayTime, VR::ReferenceSpace space);
        Misc::Pose getPredictedHeadPose(int64_t predictedDisplayTime, VR::ReferenceSpace space);
        void enablePredictions();
        void disablePredictions();
        long long getLastPredictedDisplayTime();
        long long getLastPredictedDisplayPeriod();
        std::array<SwapchainConfig, 2> getRecommendedSwapchainConfig() const;
        XrSpace getReferenceSpace(VR::ReferenceSpace space);
        XrSession xrSession() const { return mXrSession; };
        XrInstance xrInstance() const { return mXrInstance; };
        bool xrExtensionIsEnabled(const char* extensionName) const;
        void xrUpdateNames();
        PFN_xrVoidFunction xrGetFunction(const std::string& name);
        int64_t selectColorFormat();
        int64_t selectDepthFormat();
        void eraseFormat(int64_t format);
        XR::Platform& platform() { return mPlatform; }
        XR::Tracker& tracker() { return *mTracker; }
        void initTracker();
        VR::StageToWorldBinding& stageToWorldBinding() { return *mTrackerToWorldBinding; }

        enum class SwapchainUse
        {
            Color,
            Depth,
        };
        VR::Swapchain* createSwapchain(uint32_t width, uint32_t height, uint32_t samples, SwapchainUse use, const std::string& name);

        VR::Session& session() { return *mVRSession; };

    protected:
        void setupExtensionsAndLayers();
        void setupDebugMessenger(void);
        void LogInstanceInfo();
        void LogReferenceSpaces();
        void createReferenceSpaces();
        void getSystem();
        void getSystemProperties();
        void enumerateViews();
        void setupLayerDepth();

    private:

        bool initialized = false;
        bool mPredictionsEnabled = false;
        XrInstance mXrInstance = XR_NULL_HANDLE;
        XrSession mXrSession = XR_NULL_HANDLE;
        XrSpace mSpace = XR_NULL_HANDLE;
        XrFormFactor mFormFactor = XR_FORM_FACTOR_HEAD_MOUNTED_DISPLAY;
        XrViewConfigurationType mViewConfigType = XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO;
        XrEnvironmentBlendMode mEnvironmentBlendMode = XR_ENVIRONMENT_BLEND_MODE_OPAQUE;
        XrSystemId mSystemId = XR_NULL_SYSTEM_ID;
        XrSystemProperties mSystemProperties{ XR_TYPE_SYSTEM_PROPERTIES };
        std::array<XrViewConfigurationView, 2> mConfigViews{ { {XR_TYPE_VIEW_CONFIGURATION_VIEW}, {XR_TYPE_VIEW_CONFIGURATION_VIEW} } };
        XrSpace mReferenceSpaceView = XR_NULL_HANDLE;
        XrSpace mReferenceSpaceStage = XR_NULL_HANDLE;
        XrSpace mReferenceSpaceLocal = XR_NULL_HANDLE;
        XrFrameState mFrameState{};
        XrDebugUtilsMessengerEXT mDebugMessenger{ nullptr };
        Platform mPlatform;

        std::shared_ptr<XR::Tracker> mTracker{ nullptr };
        std::unique_ptr<VR::StageToWorldBinding> mTrackerToWorldBinding{ nullptr };

        uint32_t mAcquiredResources = 0;
        std::mutex mMutex{};

        std::array<XrCompositionLayerDepthInfoKHR, 2> mLayerDepth;
        std::unique_ptr<VR::Session> mVRSession;
    };

    extern const char* to_string(XrReferenceSpaceType e);
    extern const char* to_string(XrViewConfigurationType e);
    extern const char* to_string(XrEnvironmentBlendMode e);
    extern const char* to_string(XrSessionState e);
    extern const char* to_string(XrResult e);
    extern const char* to_string(XrFormFactor e);
    extern const char* to_string(XrStructureType e);
}

#endif 
