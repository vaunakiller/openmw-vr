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
        long long getLastPredictedDisplayTime();
        long long getLastPredictedDisplayPeriod();
        std::array<SwapchainConfig, 2> getRecommendedSwapchainConfig() const;
        XrInstance xrInstance() const { return mXrInstance; };
        bool xrExtensionIsEnabled(const char* extensionName) const;
        PFN_xrVoidFunction xrGetFunction(const std::string& name);
        int64_t selectColorFormat();
        int64_t selectDepthFormat();
        void eraseFormat(int64_t format);
        XR::Platform& platform() { return mPlatform; }

        VR::Session& session() { return *mVRSession; };

    protected:
        void setupExtensionsAndLayers();
        void setupDebugMessenger(void);
        void LogInstanceInfo();
        void getSystem();
        void getSystemProperties();
        void enumerateViews();

        void cleanup();
        void destroyXrInstance();

    private:

        bool initialized = false;
        XrInstance mXrInstance = XR_NULL_HANDLE;
        XrSpace mSpace = XR_NULL_HANDLE;
        XrFormFactor mFormFactor = XR_FORM_FACTOR_HEAD_MOUNTED_DISPLAY;
        XrViewConfigurationType mViewConfigType = XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO;
        XrSystemId mSystemId = XR_NULL_SYSTEM_ID;
        XrSystemProperties mSystemProperties{ XR_TYPE_SYSTEM_PROPERTIES };
        std::array<XrViewConfigurationView, 2> mConfigViews{ { {XR_TYPE_VIEW_CONFIGURATION_VIEW}, {XR_TYPE_VIEW_CONFIGURATION_VIEW} } };
        XrFrameState mFrameState{};
        XrDebugUtilsMessengerEXT mDebugMessenger{ nullptr };
        Platform mPlatform;

        uint32_t mAcquiredResources = 0;
        std::mutex mMutex{};

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
