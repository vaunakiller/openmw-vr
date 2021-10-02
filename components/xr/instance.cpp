#include "instance.hpp"
#include "debug.hpp"
#include "platform.hpp"
#include "swapchain.hpp"
#include "typeconversion.hpp"
//#include "vrenvironment.hpp"
//#include "vrinputmanager.hpp"
#include "session.hpp"

#include <components/debug/debuglog.hpp>
#include <components/sdlutil/sdlgraphicswindow.hpp>
#include <components/esm/loadrace.hpp>
#include <components/vr/directx.hpp>
#include <components/vr/layer.hpp>
#include <components/vr/trackingmanager.hpp>

#include <openxr/openxr_reflection.h>

#include <osg/Camera>

#include <vector>
#include <array>
#include <iostream>
#include <stdexcept>
#include <cassert>


namespace XR
{
#define ENUM_CASE_STR(name, val) case name: return #name;
#define MAKE_TO_STRING_FUNC(enumType)                  \
    const char* to_string(enumType e) {         \
        switch (e) {                                   \
            XR_LIST_ENUM_##enumType(ENUM_CASE_STR)     \
            default: return "Unknown " #enumType;      \
        }                                              \
    }

    MAKE_TO_STRING_FUNC(XrReferenceSpaceType)
    MAKE_TO_STRING_FUNC(XrViewConfigurationType)
    MAKE_TO_STRING_FUNC(XrEnvironmentBlendMode)
    MAKE_TO_STRING_FUNC(XrSessionState)
    MAKE_TO_STRING_FUNC(XrResult)
    MAKE_TO_STRING_FUNC(XrFormFactor)
    MAKE_TO_STRING_FUNC(XrStructureType)

    Instance* sInstance = nullptr;

    Instance& Instance::instance()
    {
        assert(sInstance);
        return *sInstance;
    }

    Instance::Instance(osg::GraphicsContext* gc)
    {
        if (!sInstance)
            sInstance = this;
        else
            throw std::logic_error("Duplicated XR::Instance singleton");


        mSystemProperties.type = XR_TYPE_SYSTEM_PROPERTIES;
        mConfigViews[0].type = XR_TYPE_VIEW_CONFIGURATION_VIEW;
        mConfigViews[1].type = XR_TYPE_VIEW_CONFIGURATION_VIEW;

        mExtensions = std::make_unique<Extensions>();
        mPlatform = std::make_unique<Platform>(gc);
        mPlatform->selectGraphicsAPIExtension();
        mXrInstance = mExtensions->createXrInstance("openmw_vr");
        Debugging::setName(mXrInstance, "OpenMW XR Instance");

        LogInstanceInfo();
        setupDebugMessenger();
        getSystem();
        getSystemProperties();
        enumerateViews();

        // TODO: Blend mode
        // setupBlendMode();
    }

    void Instance::getSystem()
    {
        XrSystemGetInfo systemInfo{};
        systemInfo.type = XR_TYPE_SYSTEM_GET_INFO;
        systemInfo.formFactor = XR_FORM_FACTOR_HEAD_MOUNTED_DISPLAY;
        auto res = CHECK_XRCMD(xrGetSystem(mXrInstance, &systemInfo, &mSystemId));
        if (!XR_SUCCEEDED(res))
            initFailure(res, mXrInstance);
    }

    void Instance::getSystemProperties()
    {
        // Read and log graphics properties for the swapchain
        CHECK_XRCMD(xrGetSystemProperties(mXrInstance, mSystemId, &mSystemProperties));

        // Log system properties.
        {
            std::stringstream ss;
            ss << "System Properties: Name=" << mSystemProperties.systemName << " VendorId=" << mSystemProperties.vendorId << std::endl;
            ss << "System Graphics Properties: MaxWidth=" << mSystemProperties.graphicsProperties.maxSwapchainImageWidth;
            ss << " MaxHeight=" << mSystemProperties.graphicsProperties.maxSwapchainImageHeight;
            ss << " MaxLayers=" << mSystemProperties.graphicsProperties.maxLayerCount << std::endl;
            ss << "System Tracking Properties: OrientationTracking=" << (mSystemProperties.trackingProperties.orientationTracking ? "True" : "False");
            ss << " PositionTracking=" << (mSystemProperties.trackingProperties.positionTracking ? "True" : "False");
            Log(Debug::Verbose) << ss.str();
        }
    }

    void Instance::enumerateViews()
    {
        uint32_t viewCount = 0;
        CHECK_XRCMD(xrEnumerateViewConfigurationViews(mXrInstance, mSystemId, mViewConfigType, 2, &viewCount, mConfigViews.data()));

        if (viewCount != 2)
        {
            std::stringstream ss;
            ss << "xrEnumerateViewConfigurationViews returned " << viewCount << " views";
            Log(Debug::Verbose) << ss.str();
        }
    }

    void Instance::cleanup()
    {
        destroyXrInstance();
    }

    void Instance::destroyXrInstance()
    {
        if (mXrInstance)
            CHECK_XRCMD(xrDestroyInstance(mXrInstance));
    }

    std::string XrResultString(XrResult res)
    {
        return to_string(res);
    }

    Instance::~Instance()
    {
    }

    void Instance::setupExtensionsAndLayers()
    {

    }
    static XrBool32 xrDebugCallback(
        XrDebugUtilsMessageSeverityFlagsEXT messageSeverity,
        XrDebugUtilsMessageTypeFlagsEXT messageType,
        const XrDebugUtilsMessengerCallbackDataEXT* callbackData,
        void* userData)
    {
        Instance* manager = reinterpret_cast<Instance*>(userData);
        (void)manager;
        std::string severityStr = "";
        std::string typeStr = "";

        switch (messageSeverity)
        {
        case XR_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT:
            severityStr = "Verbose"; break;
        case XR_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT:
            severityStr = "Info"; break;
        case XR_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT:
            severityStr = "Warning"; break;
        case XR_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT:
            severityStr = "Error"; break;
        default:
            severityStr = "Unknown"; break;
        }

        switch (messageType)
        {
        case XR_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT:
            typeStr = "General"; break;
        case XR_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT:
            typeStr = "Validation"; break;
        case XR_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT:
            typeStr = "Performance"; break;
        case XR_DEBUG_UTILS_MESSAGE_TYPE_CONFORMANCE_BIT_EXT:
            typeStr = "Conformance"; break;
        default:
            typeStr = "Unknown"; break;
        }

        Log(Debug::Verbose) << "XrCallback: [" << severityStr << "][" << typeStr << "][ID=" << (callbackData->messageId ? callbackData->messageId : "null") << "]: " << callbackData->message;

        return XR_FALSE;
    }

    void Instance::setupDebugMessenger(void)
    {
        if (mExtensions->extensionEnabled(XR_EXT_DEBUG_UTILS_EXTENSION_NAME))
        {
            XrDebugUtilsMessengerCreateInfoEXT createInfo{};
            createInfo.type = XR_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;

            // Debug message severity levels
            if (Settings::Manager::getBool("XR_EXT_debug_utils message level verbose", "VR Debug"))
                createInfo.messageSeverities |= XR_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT;
            if (Settings::Manager::getBool("XR_EXT_debug_utils message level info", "VR Debug"))
                createInfo.messageSeverities |= XR_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT;
            if (Settings::Manager::getBool("XR_EXT_debug_utils message level warning", "VR Debug"))
                createInfo.messageSeverities |= XR_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT;
            if (Settings::Manager::getBool("XR_EXT_debug_utils message level error", "VR Debug"))
                createInfo.messageSeverities |= XR_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;

            // Debug message types
            if (Settings::Manager::getBool("XR_EXT_debug_utils message type general", "VR Debug"))
                createInfo.messageTypes |= XR_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT;
            if (Settings::Manager::getBool("XR_EXT_debug_utils message type validation", "VR Debug"))
                createInfo.messageTypes |= XR_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT;
            if (Settings::Manager::getBool("XR_EXT_debug_utils message type performance", "VR Debug"))
                createInfo.messageTypes |= XR_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
            if (Settings::Manager::getBool("XR_EXT_debug_utils message type conformance", "VR Debug"))
                createInfo.messageTypes |= XR_DEBUG_UTILS_MESSAGE_TYPE_CONFORMANCE_BIT_EXT;

            createInfo.userCallback = &xrDebugCallback;
            createInfo.userData = this;

            PFN_xrCreateDebugUtilsMessengerEXT createDebugUtilsMessenger = reinterpret_cast<PFN_xrCreateDebugUtilsMessengerEXT>(xrGetFunction("xrCreateDebugUtilsMessengerEXT"));
            assert(createDebugUtilsMessenger);
            CHECK_XRCMD(createDebugUtilsMessenger(mXrInstance, &createInfo, &mDebugMessenger));
        }
    }

    void
        Instance::LogInstanceInfo() {

        XrInstanceProperties instanceProperties{};
        instanceProperties.type = XR_TYPE_INSTANCE_PROPERTIES;

        CHECK_XRCMD(xrGetInstanceProperties(mXrInstance, &instanceProperties));
        Log(Debug::Verbose) << "Instance RuntimeName=" << instanceProperties.runtimeName << " RuntimeVersion=" << instanceProperties.runtimeVersion;
    }

    void
        Instance::endFrame(VR::Frame& frame)
    {
    }

    PFN_xrVoidFunction Instance::xrGetFunction(const std::string& name)
    {
        PFN_xrVoidFunction function = nullptr;
        xrGetInstanceProcAddr(mXrInstance, name.c_str(), &function);
        return function;
    }

    int64_t Instance::selectColorFormat()
    {
        // Find supported color swapchain format.
        return mPlatform->selectColorFormat();
    }

    int64_t Instance::selectDepthFormat()
    {
        // Find supported depth swapchain format.
        return mPlatform->selectDepthFormat();
    }

    void Instance::eraseFormat(int64_t format)
    {
        mPlatform->eraseFormat(format);
    }

    XR::Platform& Instance::platform()
    {
        return *mPlatform;
    }

    std::unique_ptr<VR::Session> Instance::createSession()
    {
        auto xrSession = mPlatform->createXrSession(mXrInstance, mSystemId);
        return std::make_unique<Session>(xrSession, mViewConfigType);
    }

    std::array<XrViewConfigurationView, 2> Instance::getRecommendedXrSwapchainConfig() const
    {
        return mConfigViews;
    }
}

