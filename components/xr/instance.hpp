#ifndef XR_INSTANCE_HPP
#define XR_INSTANCE_HPP

#include "extensions.hpp"
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


    /// \brief Instantiates and manages and openxr instance.
    class Instance
    {
    public:
        static Instance& instance();

    public:
        Instance(osg::GraphicsContext* gc);
        ~Instance(void);

        void endFrame(VR::Frame& frame);
        std::array<XrViewConfigurationView, 2> getRecommendedXrSwapchainConfig() const;
        XrInstance xrInstance() const { return mXrInstance; };
        PFN_xrVoidFunction xrGetFunction(const std::string& name);
        int64_t selectColorFormat();
        int64_t selectDepthFormat();
        void eraseFormat(int64_t format);
        XR::Platform& platform();

        std::unique_ptr<VR::Session> createSession();

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

        XrInstance mXrInstance = XR_NULL_HANDLE;
        XrViewConfigurationType mViewConfigType = XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO;
        XrSystemId mSystemId = XR_NULL_SYSTEM_ID;
        XrSystemProperties mSystemProperties;
        std::array<XrViewConfigurationView, 2> mConfigViews;
        XrDebugUtilsMessengerEXT mDebugMessenger{ nullptr };
        std::unique_ptr<Extensions> mExtensions;
        std::unique_ptr<Platform> mPlatform;

        // TODO: uint32_t mAcquiredResources = 0;
        std::mutex mMutex{};
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
