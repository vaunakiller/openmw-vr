#include "openxrmanagerimpl.hpp"
#include "openxrdebug.hpp"
#include "openxrswapchain.hpp"
#include "openxrswapchainimpl.hpp"

#include <components/debug/debuglog.hpp>
#include <components/sdlutil/sdlgraphicswindow.hpp>
#include <components/esm/loadrace.hpp>

#include "../mwmechanics/actorutil.hpp"

#include "../mwbase/world.hpp"
#include "../mwbase/environment.hpp"

#include "../mwworld/class.hpp"
#include "../mwworld/player.hpp"
#include "../mwworld/esmstore.hpp"

// The OpenXR SDK's platform headers assume we've included platform headers
#ifdef _WIN32
#include <Windows.h>
#include <objbase.h>

#elif __linux__
#include <X11/Xlib.h>
#include <GL/glx.h>
#undef None

#else
#error Unsupported platform
#endif

#include <openxr/openxr_platform.h>
#include <openxr/openxr_platform_defines.h>
#include <openxr/openxr_reflection.h>

#include <osg/Camera>

#include <vector>
#include <array>
#include <iostream>

#define ENUM_CASE_STR(name, val) case name: return #name;
#define MAKE_TO_STRING_FUNC(enumType)                  \
    inline const char* to_string(enumType e) {         \
        switch (e) {                                   \
            XR_LIST_ENUM_##enumType(ENUM_CASE_STR)     \
            default: return "Unknown " #enumType;      \
        }                                              \
    }

MAKE_TO_STRING_FUNC(XrReferenceSpaceType);
MAKE_TO_STRING_FUNC(XrViewConfigurationType);
MAKE_TO_STRING_FUNC(XrEnvironmentBlendMode);
MAKE_TO_STRING_FUNC(XrSessionState);
MAKE_TO_STRING_FUNC(XrResult);
MAKE_TO_STRING_FUNC(XrFormFactor);
MAKE_TO_STRING_FUNC(XrStructureType);

namespace MWVR
{
    OpenXRManagerImpl::OpenXRManagerImpl()
    {
        logLayersAndExtensions();
        setupExtensionsAndLayers();

        std::vector<const char*> extensions;
        for (auto& extension : mEnabledExtensions)
            extensions.push_back(extension.c_str());


        { // Create Instance
            XrInstanceCreateInfo createInfo{ XR_TYPE_INSTANCE_CREATE_INFO };
            createInfo.next = nullptr;
            createInfo.enabledExtensionCount = extensions.size();
            createInfo.enabledExtensionNames = extensions.data();
            strcpy(createInfo.applicationInfo.applicationName, "openmw_vr");
            createInfo.applicationInfo.apiVersion = XR_CURRENT_API_VERSION;
            CHECK_XRCMD(xrCreateInstance(&createInfo, &mInstance));
            assert(mInstance);
        }

        setupDebugMessenger();

        {
            // Layer depth is enabled, cache the invariant values
            if (xrExtensionIsEnabled(XR_KHR_COMPOSITION_LAYER_DEPTH_EXTENSION_NAME))
            {
                GLfloat depthRange[2] = { 0.f, 1.f };
                glGetFloatv(GL_DEPTH_RANGE, depthRange);
                auto nearClip = Settings::Manager::getFloat("near clip", "Camera");

                for (auto& layer : mLayerDepth)
                {
                    layer.type = XR_TYPE_COMPOSITION_LAYER_DEPTH_INFO_KHR;
                    layer.next = nullptr;
                    layer.minDepth = depthRange[0];
                    layer.maxDepth = depthRange[1];
                    layer.nearZ = nearClip;
                }
            }
        }

        { // Get system ID
            XrSystemGetInfo systemInfo{ XR_TYPE_SYSTEM_GET_INFO };
            systemInfo.formFactor = mFormFactor = XR_FORM_FACTOR_HEAD_MOUNTED_DISPLAY;
            CHECK_XRCMD(xrGetSystem(mInstance, &systemInfo, &mSystemId));
            assert(mSystemId);
        }

        { // Initialize OpenGL device
            // Despite its name, xrGetOpenGLGraphicsRequirementsKHR is a required function call that sets up an opengl instance.
            PFN_xrGetOpenGLGraphicsRequirementsKHR p_getRequirements = nullptr;
            xrGetInstanceProcAddr(mInstance, "xrGetOpenGLGraphicsRequirementsKHR", reinterpret_cast<PFN_xrVoidFunction*>(&p_getRequirements));
            XrGraphicsRequirementsOpenGLKHR requirements{ XR_TYPE_GRAPHICS_REQUIREMENTS_OPENGL_KHR };
            CHECK_XRCMD(p_getRequirements(mInstance, mSystemId, &requirements));

            const XrVersion desiredApiVersion = XR_MAKE_VERSION(4, 6, 0);
            if (requirements.minApiVersionSupported > desiredApiVersion) {
                std::cout << "Runtime does not support desired Graphics API and/or version" << std::endl;
            }
        }

#ifdef _WIN32
        { // Create Session
            auto DC = wglGetCurrentDC();
            auto GLRC = wglGetCurrentContext();
            auto XRGLRC = wglCreateContext(DC);
            wglShareLists(GLRC, XRGLRC);

            XrGraphicsBindingOpenGLWin32KHR graphicsBindings;
            graphicsBindings.type = XR_TYPE_GRAPHICS_BINDING_OPENGL_WIN32_KHR;
            graphicsBindings.next = nullptr;
            graphicsBindings.hDC = DC;
            graphicsBindings.hGLRC = XRGLRC;

            if (!graphicsBindings.hDC)
                Log(Debug::Warning) << "Missing DC";

            XrSessionCreateInfo createInfo{ XR_TYPE_SESSION_CREATE_INFO };
            createInfo.next = &graphicsBindings;
            createInfo.systemId = mSystemId;
            CHECK_XRCMD(xrCreateSession(mInstance, &createInfo, &mSession));
            assert(mSession);
        }
#elif __linux__
        { // Create Session
            Display* xDisplay = XOpenDisplay(NULL);
            GLXContext glxContext = glXGetCurrentContext();
            GLXDrawable glxDrawable = glXGetCurrentDrawable();

            // TODO: runtimes don't actually care (yet)
            GLXFBConfig glxFBConfig = 0;
            uint32_t visualid = 0;

            XrGraphicsBindingOpenGLXlibKHR graphicsBindings;
            graphicsBindings.type = XR_TYPE_GRAPHICS_BINDING_OPENGL_XLIB_KHR;
            graphicsBindings.next = nullptr;
            graphicsBindings.xDisplay = xDisplay;
            graphicsBindings.glxContext = glxContext;
            graphicsBindings.glxDrawable = glxDrawable;
            graphicsBindings.glxFBConfig = glxFBConfig;
            graphicsBindings.visualid = visualid;

            if (!graphicsBindings.glxContext)
                Log(Debug::Warning) << "Missing glxContext";

            if (!graphicsBindings.glxDrawable)
                Log(Debug::Warning) << "Missing glxDrawable";

            XrSessionCreateInfo createInfo{ XR_TYPE_SESSION_CREATE_INFO };
            createInfo.next = &graphicsBindings;
            createInfo.systemId = mSystemId;
            CHECK_XRCMD(xrCreateSession(mInstance, &createInfo, &mSession));
            assert(mSession);
        }
#endif

        LogInstanceInfo();
        LogReferenceSpaces();
        LogSwapchainFormats();

        { // Set up reference space
            XrReferenceSpaceCreateInfo createInfo{ XR_TYPE_REFERENCE_SPACE_CREATE_INFO };
            createInfo.poseInReferenceSpace.orientation.w = 1.f; // Identity pose
            createInfo.referenceSpaceType = XR_REFERENCE_SPACE_TYPE_VIEW;
            CHECK_XRCMD(xrCreateReferenceSpace(mSession, &createInfo, &mReferenceSpaceView));
            createInfo.referenceSpaceType = XR_REFERENCE_SPACE_TYPE_STAGE;
            CHECK_XRCMD(xrCreateReferenceSpace(mSession, &createInfo, &mReferenceSpaceStage));
        }

        { // Read and log graphics properties for the swapchain
            xrGetSystemProperties(mInstance, mSystemId, &mSystemProperties);

            // Log system properties.
            {
                std::stringstream ss;
                ss << "System Properties: Name=" << mSystemProperties.systemName << " VendorId=" << mSystemProperties.vendorId << std::endl;
                ss << "System Graphics Properties: MaxWidth=" << mSystemProperties.graphicsProperties.maxSwapchainImageWidth;
                ss << " MaxHeight=" << mSystemProperties.graphicsProperties.maxSwapchainImageHeight;
                ss << " MaxLayers=" << mSystemProperties.graphicsProperties.maxLayerCount << std::endl;
                ss << "System Tracking Properties: OrientationTracking=" << mSystemProperties.trackingProperties.orientationTracking ? "True" : "False";
                ss << " PositionTracking=" << mSystemProperties.trackingProperties.positionTracking ? "True" : "False";
                Log(Debug::Verbose) << ss.str();
            }

            uint32_t viewCount = 0;
            CHECK_XRCMD(xrEnumerateViewConfigurationViews(mInstance, mSystemId, mViewConfigType, 2, &viewCount, mConfigViews.data()));

            if (viewCount != 2)
            {
                std::stringstream ss;
                ss << "xrEnumerateViewConfigurationViews returned " << viewCount << " views";
                Log(Debug::Verbose) << ss.str();
            }
        }
    }

    inline XrResult CheckXrResult(XrResult res, const char* originator, const char* sourceLocation) {
        static bool initialized = false;
        static bool sLogAllXrCalls = false;
        static bool sContinueOnErrors = false;
        if (!initialized)
        {
            initialized = true;
            sLogAllXrCalls = Settings::Manager::getBool("log all openxr calls", "VR Debug");
            sContinueOnErrors = Settings::Manager::getBool("continue on errors", "VR Debug");
        }

        if (XR_FAILED(res)) {
            std::stringstream ss;
#ifdef _WIN32
            ss << sourceLocation << ": OpenXR[Error: " << to_string(res) << "][Thread: " << std::this_thread::get_id() << "][DC: " << wglGetCurrentDC() << "][GLRC: " << wglGetCurrentContext() << "]: " << originator;
#elif __linux__
            ss << sourceLocation << ": OpenXR[Error: " << to_string(res) << "][Thread: " << std::this_thread::get_id() << "][glxContext: " << glXGetCurrentContext() << "][glxDrawable: " << glXGetCurrentDrawable() << "]: " << originator;
#endif
            Log(Debug::Error) << ss.str();
            if (res == XR_ERROR_TIME_INVALID)
                Log(Debug::Error) << "Breakpoint";
            if (!sContinueOnErrors)
                throw std::runtime_error(ss.str().c_str());
        }
        else if (res != XR_SUCCESS || sLogAllXrCalls)
        {
#ifdef _WIN32
            Log(Debug::Verbose) << sourceLocation << ": OpenXR[" << to_string(res) << "][" << std::this_thread::get_id() << "][" << wglGetCurrentDC() << "][" << wglGetCurrentContext() << "]: " << originator;
#elif __linux__
            Log(Debug::Verbose) << sourceLocation << ": OpenXR[" << to_string(res) << "][" << std::this_thread::get_id() << "][" << glXGetCurrentContext() << "][" << glXGetCurrentDrawable() << "]: " << originator;
#endif
        }

        return res;
    }

    std::string XrResultString(XrResult res)
    {
        return to_string(res);
    }

    OpenXRManagerImpl::~OpenXRManagerImpl()
    {

    }


    static std::vector<std::string> enumerateExtensions(const char* layerName, bool log = false, int logIndent = 2)
    {
        uint32_t extensionCount = 0;
        std::vector<XrExtensionProperties> availableExtensions;
        xrEnumerateInstanceExtensionProperties(layerName, 0, &extensionCount, nullptr);
        availableExtensions.resize(extensionCount, XrExtensionProperties{ XR_TYPE_EXTENSION_PROPERTIES });
        xrEnumerateInstanceExtensionProperties(layerName, availableExtensions.size(), &extensionCount, availableExtensions.data());

        std::vector<std::string> extensionNames;
        const std::string indentStr(logIndent, ' ');
        for (auto& extension : availableExtensions)
        {
            extensionNames.push_back(extension.extensionName);
            if (log)
                Log(Debug::Verbose) << indentStr << "Name=" << extension.extensionName << " SpecVersion=" << extension.extensionVersion;
        }
        return extensionNames;
    }

#if    !XR_KHR_composition_layer_depth \
    || !XR_KHR_opengl_enable \
    || !XR_EXT_hp_mixed_reality_controller \
    || !XR_EXT_debug_utils

#error "OpenXR extensions missing. Please upgrade your copy of the OpenXR SDK to 1.0.12 minimum"
#endif

    void OpenXRManagerImpl::setupExtensionsAndLayers()
    {
        std::vector<const char*> requiredExtensions = {
            XR_KHR_OPENGL_ENABLE_EXTENSION_NAME,
        };

        std::vector<const char*> optionalExtensions = {
            XR_KHR_COMPOSITION_LAYER_DEPTH_EXTENSION_NAME,
            XR_EXT_HP_MIXED_REALITY_CONTROLLER_EXTENSION_NAME
        };

        if (Settings::Manager::getBool("enable XR_EXT_debug_utils", "VR Debug"))
            optionalExtensions.emplace_back(XR_EXT_DEBUG_UTILS_EXTENSION_NAME);

        for (auto& extension : enumerateExtensions(nullptr))
            mAvailableExtensions.insert(extension);

        for (auto requiredExtension : requiredExtensions)
            enableExtension(requiredExtension, false);

        for (auto optionalExtension : optionalExtensions)
            enableExtension(optionalExtension, true);

    }

    void OpenXRManagerImpl::enableExtension(const std::string& extension, bool optional)
    {
        if (mAvailableExtensions.count(extension) > 0)
        {
            Log(Debug::Verbose) << "  " << extension << ": enabled";
            mEnabledExtensions.insert(extension);
        }
        else
        {
            Log(Debug::Verbose) << "  " << extension << ": disabled (not supported)";
            if (!optional)
            {
                throw std::runtime_error(std::string("Required OpenXR extension ") + extension + " not supported by the runtime");
            }
        }
    }

    static XrBool32 xrDebugCallback(
        XrDebugUtilsMessageSeverityFlagsEXT messageSeverity,
        XrDebugUtilsMessageTypeFlagsEXT messageType,
        const XrDebugUtilsMessengerCallbackDataEXT* callbackData,
        void* userData)
    {
        OpenXRManagerImpl* manager = reinterpret_cast<OpenXRManagerImpl*>(userData);
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

    void OpenXRManagerImpl::setupDebugMessenger(void)
    {
        if (xrExtensionIsEnabled(XR_EXT_DEBUG_UTILS_EXTENSION_NAME))
        {
            XrDebugUtilsMessengerCreateInfoEXT createInfo{ XR_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT, nullptr };

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
            CHECK_XRCMD(createDebugUtilsMessenger(mInstance, &createInfo, &mDebugMessenger));
        }
    }

    void
        OpenXRManagerImpl::logLayersAndExtensions() {
        // Log layers and any of their extensions.
        uint32_t layerCount;
        CHECK_XRCMD(xrEnumerateApiLayerProperties(0, &layerCount, nullptr));
        std::vector<XrApiLayerProperties> layers(layerCount, XrApiLayerProperties{ XR_TYPE_API_LAYER_PROPERTIES });
        CHECK_XRCMD(xrEnumerateApiLayerProperties((uint32_t)layers.size(), &layerCount, layers.data()));

        Log(Debug::Verbose) << "Available Extensions: ";
        enumerateExtensions(nullptr, true, 2);
        Log(Debug::Verbose) << "Available Layers: ";

        for (const XrApiLayerProperties& layer : layers) {
            Log(Debug::Verbose) << "  Name=" << layer.layerName << " SpecVersion=" << layer.layerVersion;
            enumerateExtensions(layer.layerName, true, 4);
        }
    }

    void
        OpenXRManagerImpl::LogInstanceInfo() {

        XrInstanceProperties instanceProperties{ XR_TYPE_INSTANCE_PROPERTIES };
        xrGetInstanceProperties(mInstance, &instanceProperties);
        Log(Debug::Verbose) << "Instance RuntimeName=" << instanceProperties.runtimeName << " RuntimeVersion=" << instanceProperties.runtimeVersion;
    }

    void
        OpenXRManagerImpl::LogReferenceSpaces() {

        uint32_t spaceCount;
        xrEnumerateReferenceSpaces(mSession, 0, &spaceCount, nullptr);
        std::vector<XrReferenceSpaceType> spaces(spaceCount);
        xrEnumerateReferenceSpaces(mSession, spaceCount, &spaceCount, spaces.data());

        std::stringstream ss;
        ss << "Available reference spaces=" << spaceCount << std::endl;

        for (XrReferenceSpaceType space : spaces)
            ss << "  Name: " << to_string(space) << std::endl;
        Log(Debug::Verbose) << ss.str();
    }

    void OpenXRManagerImpl::LogSwapchainFormats()
    {
        uint32_t swapchainFormatCount;
        CHECK_XRCMD(xrEnumerateSwapchainFormats(xrSession(), 0, &swapchainFormatCount, nullptr));
        std::vector<int64_t> swapchainFormats(swapchainFormatCount);
        CHECK_XRCMD(xrEnumerateSwapchainFormats(xrSession(), (uint32_t)swapchainFormats.size(), &swapchainFormatCount, swapchainFormats.data()));

        std::stringstream ss;
        ss << "Available Swapchain formats: (" << swapchainFormatCount << ")" << std::endl;

        for (auto format : swapchainFormats)
        {
            ss << "  Enum=" << std::dec << format << " (0x=" << std::hex << format << ")" << std::dec << std::endl;
        }

        Log(Debug::Verbose) << ss.str();
    }

    FrameInfo
        OpenXRManagerImpl::waitFrame()
    {
        XrFrameWaitInfo frameWaitInfo{ XR_TYPE_FRAME_WAIT_INFO };
        XrFrameState frameState{ XR_TYPE_FRAME_STATE };

        CHECK_XRCMD(xrWaitFrame(mSession, &frameWaitInfo, &frameState));
        mFrameState = frameState;

        FrameInfo frameInfo;

        frameInfo.runtimePredictedDisplayTime = mFrameState.predictedDisplayTime;
        frameInfo.runtimePredictedDisplayPeriod = mFrameState.predictedDisplayPeriod;
        frameInfo.runtimeRequestsRender = !!mFrameState.shouldRender;

        return frameInfo;
    }

    static void clearGlErrors()
    {
        auto error = glGetError();
        while (error != GL_NO_ERROR)
        {
            Log(Debug::Warning) << "glGetError: " << std::dec << error << " (0x" << std::hex << error << ")" << std::dec;
        }
    }

    void
        OpenXRManagerImpl::beginFrame()
    {
        XrFrameBeginInfo frameBeginInfo{ XR_TYPE_FRAME_BEGIN_INFO };
        CHECK_XRCMD(xrBeginFrame(mSession, &frameBeginInfo));
    }

    XrCompositionLayerProjectionView toXR(MWVR::CompositionLayerProjectionView layer)
    {
        XrCompositionLayerProjectionView xrLayer;
        xrLayer.type = XR_TYPE_COMPOSITION_LAYER_PROJECTION_VIEW;
        xrLayer.subImage = layer.swapchain->impl().xrSubImage();
        xrLayer.pose = toXR(layer.pose);
        xrLayer.fov = toXR(layer.fov);
        xrLayer.next = nullptr;

        return xrLayer;
    }

    void
        OpenXRManagerImpl::endFrame(FrameInfo frameInfo, const std::array<CompositionLayerProjectionView, 2>* layerStack)
    {

        XrFrameEndInfo frameEndInfo{ XR_TYPE_FRAME_END_INFO };
        frameEndInfo.displayTime = frameInfo.runtimePredictedDisplayTime;
        frameEndInfo.environmentBlendMode = mEnvironmentBlendMode;
        if (layerStack)
        {
            std::array<XrCompositionLayerProjectionView, 2> compositionLayerProjectionViews{};
            compositionLayerProjectionViews[(int)Side::LEFT_SIDE] = toXR((*layerStack)[(int)Side::LEFT_SIDE]);
            compositionLayerProjectionViews[(int)Side::RIGHT_SIDE] = toXR((*layerStack)[(int)Side::RIGHT_SIDE]);
            XrCompositionLayerProjection layer{};
            layer.type = XR_TYPE_COMPOSITION_LAYER_PROJECTION;
            layer.space = getReferenceSpace(ReferenceSpace::STAGE);
            layer.viewCount = 2;
            layer.views = compositionLayerProjectionViews.data();
            auto* xrLayerStack = reinterpret_cast<XrCompositionLayerBaseHeader*>(&layer);

            std::array<XrCompositionLayerDepthInfoKHR, 2> compositionLayerDepth = mLayerDepth;
            if (xrExtensionIsEnabled(XR_KHR_COMPOSITION_LAYER_DEPTH_EXTENSION_NAME))
            {
                auto farClip = Settings::Manager::getFloat("viewing distance", "Camera");
                // All values not set here are set previously as they are constant
                compositionLayerDepth[(int)Side::LEFT_SIDE].farZ = farClip;
                compositionLayerDepth[(int)Side::RIGHT_SIDE].farZ = farClip;
                compositionLayerDepth[(int)Side::LEFT_SIDE].subImage = (*layerStack)[(int)Side::LEFT_SIDE].swapchain->impl().xrSubImageDepth();
                compositionLayerDepth[(int)Side::RIGHT_SIDE].subImage = (*layerStack)[(int)Side::RIGHT_SIDE].swapchain->impl().xrSubImageDepth();
                if (compositionLayerDepth[(int)Side::LEFT_SIDE].subImage.swapchain != XR_NULL_HANDLE
                    && compositionLayerDepth[(int)Side::RIGHT_SIDE].subImage.swapchain != XR_NULL_HANDLE)
                {
                    compositionLayerProjectionViews[(int)Side::LEFT_SIDE].next = &compositionLayerDepth[(int)Side::LEFT_SIDE];
                    compositionLayerProjectionViews[(int)Side::RIGHT_SIDE].next = &compositionLayerDepth[(int)Side::RIGHT_SIDE];
                }
            }
            frameEndInfo.layerCount = 1;
            frameEndInfo.layers = &xrLayerStack;
        }
        CHECK_XRCMD(xrEndFrame(mSession, &frameEndInfo));
    }

    std::array<View, 2>
        OpenXRManagerImpl::getPredictedViews(
            int64_t predictedDisplayTime,
            ReferenceSpace space)
    {
        if (!mPredictionsEnabled)
        {
            Log(Debug::Error) << "Prediction out of order";
            throw std::logic_error("Prediction out of order");
        }
        std::array<XrView, 2> xrViews{ {{XR_TYPE_VIEW}, {XR_TYPE_VIEW}} };
        XrViewState viewState{ XR_TYPE_VIEW_STATE };
        uint32_t viewCount = 2;

        XrViewLocateInfo viewLocateInfo{ XR_TYPE_VIEW_LOCATE_INFO };
        viewLocateInfo.viewConfigurationType = mViewConfigType;
        viewLocateInfo.displayTime = predictedDisplayTime;
        switch (space)
        {
        case ReferenceSpace::STAGE:
            viewLocateInfo.space = mReferenceSpaceStage;
            break;
        case ReferenceSpace::VIEW:
            viewLocateInfo.space = mReferenceSpaceView;
            break;
        }
        CHECK_XRCMD(xrLocateViews(mSession, &viewLocateInfo, &viewState, viewCount, &viewCount, xrViews.data()));

        std::array<View, 2> vrViews{};
        vrViews[(int)Side::LEFT_SIDE].pose = fromXR(xrViews[(int)Side::LEFT_SIDE].pose);
        vrViews[(int)Side::RIGHT_SIDE].pose = fromXR(xrViews[(int)Side::RIGHT_SIDE].pose);
        vrViews[(int)Side::LEFT_SIDE].fov = fromXR(xrViews[(int)Side::LEFT_SIDE].fov);
        vrViews[(int)Side::RIGHT_SIDE].fov = fromXR(xrViews[(int)Side::RIGHT_SIDE].fov);
        return vrViews;
    }

    MWVR::Pose OpenXRManagerImpl::getPredictedHeadPose(
        int64_t predictedDisplayTime,
        ReferenceSpace space)
    {
        if (!mPredictionsEnabled)
        {
            Log(Debug::Error) << "Prediction out of order";
            throw std::logic_error("Prediction out of order");
        }
        XrSpaceLocation location{ XR_TYPE_SPACE_LOCATION };
        XrSpace limbSpace = mReferenceSpaceView;
        XrSpace referenceSpace = XR_NULL_HANDLE;

        switch (space)
        {
        case ReferenceSpace::STAGE:
            referenceSpace = mReferenceSpaceStage;
            break;
        case ReferenceSpace::VIEW:
            referenceSpace = mReferenceSpaceView;
            break;
        }
        CHECK_XRCMD(xrLocateSpace(limbSpace, referenceSpace, predictedDisplayTime, &location));

        if (!location.locationFlags & XR_SPACE_LOCATION_ORIENTATION_VALID_BIT)
        {
            // Quat must have a magnitude of 1 but openxr sets it to 0 when tracking is unavailable.
            // I want a no-track pose to still be valid
            location.pose.orientation.w = 1;
        }
        return MWVR::Pose{
            fromXR(location.pose.position),
            fromXR(location.pose.orientation)
        };
    }

    void OpenXRManagerImpl::handleEvents()
    {
        std::unique_lock<std::mutex> lock(mMutex);

        xrQueueEvents();

        while (auto* event = nextEvent())
        {
            if (!processEvent(event))
            {
                // Do not consider processing an event optional.
                // Retry once per frame until every event has been successfully processed
                return;
            }
            popEvent();
        }

        if (mXrSessionShouldStop)
        {
            if (checkStopCondition())
            {
                CHECK_XRCMD(xrEndSession(mSession));
                mXrSessionShouldStop = false;
            }
        }
    }

    const XrEventDataBaseHeader* OpenXRManagerImpl::nextEvent()
    {
        if (mEventQueue.size() > 0)
            return reinterpret_cast<XrEventDataBaseHeader*> (&mEventQueue.front());
        return nullptr;
    }

    bool OpenXRManagerImpl::processEvent(const XrEventDataBaseHeader* header)
    {
        Log(Debug::Verbose) << "OpenXR: Event received: " << to_string(header->type);
        switch (header->type)
        {
        case XR_TYPE_EVENT_DATA_SESSION_STATE_CHANGED:
        {
            const auto* stateChangeEvent = reinterpret_cast<const XrEventDataSessionStateChanged*>(header);
            return handleSessionStateChanged(*stateChangeEvent);
            break;
        }
        case XR_TYPE_EVENT_DATA_INSTANCE_LOSS_PENDING:
        case XR_TYPE_EVENT_DATA_INTERACTION_PROFILE_CHANGED:
        case XR_TYPE_EVENT_DATA_REFERENCE_SPACE_CHANGE_PENDING:
        default:
        {
            Log(Debug::Verbose) << "OpenXR: Event ignored";
            break;
        }
        }
        return true;
    }

    bool
        OpenXRManagerImpl::handleSessionStateChanged(
            const XrEventDataSessionStateChanged& stateChangedEvent)
    {
        Log(Debug::Verbose) << "XrEventDataSessionStateChanged: state " << to_string(mSessionState) << "->" << to_string(stateChangedEvent.state);
        mSessionState = stateChangedEvent.state;

        // Ref: https://www.khronos.org/registry/OpenXR/specs/1.0/html/xrspec.html#session-states
        switch (mSessionState)
        {
        case XR_SESSION_STATE_IDLE:
        {
            mAppShouldSyncFrameLoop = false;
            mAppShouldRender = false;
            mAppShouldReadInput = false;
            mXrSessionShouldStop = false;
            break;
        }
        case XR_SESSION_STATE_READY:
        {
            mAppShouldSyncFrameLoop = true;
            mAppShouldRender = false;
            mAppShouldReadInput = false;
            mXrSessionShouldStop = false;

            XrSessionBeginInfo beginInfo{ XR_TYPE_SESSION_BEGIN_INFO };
            beginInfo.primaryViewConfigurationType = mViewConfigType;
            CHECK_XRCMD(xrBeginSession(mSession, &beginInfo));

            break;
        }
        case XR_SESSION_STATE_STOPPING:
        {
            mAppShouldSyncFrameLoop = false;
            mAppShouldRender = false;
            mAppShouldReadInput = false;
            mXrSessionShouldStop = true;
            break;
        }
        case XR_SESSION_STATE_SYNCHRONIZED:
        {
            mAppShouldSyncFrameLoop = true;
            mAppShouldRender = false;
            mAppShouldReadInput = false;
            mXrSessionShouldStop = false;
            break;
        }
        case XR_SESSION_STATE_VISIBLE:
        {
            mAppShouldSyncFrameLoop = true;
            mAppShouldRender = true;
            mAppShouldReadInput = false;
            mXrSessionShouldStop = false;
            break;
        }
        case XR_SESSION_STATE_FOCUSED:
        {
            mAppShouldSyncFrameLoop = true;
            mAppShouldRender = true;
            mAppShouldReadInput = true;
            mXrSessionShouldStop = false;
            break;
        }
        default:
            Log(Debug::Warning) << "XrEventDataSessionStateChanged: Ignoring new state " << to_string(mSessionState);
        }

        return true;
    }

    bool OpenXRManagerImpl::checkStopCondition()
    {
        return mAcquiredResources == 0;
    }

    bool OpenXRManagerImpl::xrNextEvent(XrEventDataBuffer& eventBuffer)
    {
        XrEventDataBaseHeader* baseHeader = reinterpret_cast<XrEventDataBaseHeader*>(&eventBuffer);
        *baseHeader = { XR_TYPE_EVENT_DATA_BUFFER };
        const XrResult result = xrPollEvent(mInstance, &eventBuffer);
        if (result == XR_SUCCESS)
        {
            if (baseHeader->type == XR_TYPE_EVENT_DATA_EVENTS_LOST) {
                const XrEventDataEventsLost* const eventsLost = reinterpret_cast<const XrEventDataEventsLost*>(baseHeader);
                Log(Debug::Warning) << "OpenXRManagerImpl: Lost " << eventsLost->lostEventCount << " events";
            }

            return baseHeader;
        }

        if (result != XR_EVENT_UNAVAILABLE)
            CHECK_XRRESULT(result, "xrPollEvent");
        return false;
    }

    void OpenXRManagerImpl::popEvent()
    {
        if (mEventQueue.size() > 0)
            mEventQueue.pop();
    }

    void
        OpenXRManagerImpl::xrQueueEvents()
    {
        XrEventDataBuffer eventBuffer;
        while (xrNextEvent(eventBuffer))
        {
            mEventQueue.push(eventBuffer);
        }
    }

    MWVR::Pose fromXR(XrPosef pose)
    {
        return MWVR::Pose{ fromXR(pose.position), fromXR(pose.orientation) };
    }

    XrPosef toXR(MWVR::Pose pose)
    {
        return XrPosef{ toXR(pose.orientation), toXR(pose.position) };
    }

    MWVR::FieldOfView fromXR(XrFovf fov)
    {
        return MWVR::FieldOfView{ fov.angleLeft, fov.angleRight, fov.angleUp, fov.angleDown };
    }

    XrFovf toXR(MWVR::FieldOfView fov)
    {
        return XrFovf{ fov.angleLeft, fov.angleRight, fov.angleUp, fov.angleDown };
    }

    XrSpace OpenXRManagerImpl::getReferenceSpace(ReferenceSpace space)
    {
        XrSpace referenceSpace = XR_NULL_HANDLE;
        if (space == ReferenceSpace::STAGE)
            referenceSpace = mReferenceSpaceStage;
        if (space == ReferenceSpace::VIEW)
            referenceSpace = mReferenceSpaceView;
        return referenceSpace;
    }

    bool OpenXRManagerImpl::xrExtensionIsEnabled(const char* extensionName) const
    {
        return mEnabledExtensions.count(extensionName) != 0;
    }

    void OpenXRManagerImpl::xrResourceAcquired()
    {
        std::unique_lock<std::mutex> lock(mMutex);
        mAcquiredResources++;
    }

    void OpenXRManagerImpl::xrResourceReleased()
    {
        std::unique_lock<std::mutex> lock(mMutex);
        if (mAcquiredResources == 0)
            throw std::logic_error("Releasing a nonexistent resource");
        mAcquiredResources--;
    }

    void OpenXRManagerImpl::xrUpdateNames()
    {
        VrDebug::setName(mInstance, "OpenMW XR Instance");
        VrDebug::setName(mSession, "OpenMW XR Session");
        VrDebug::setName(mReferenceSpaceStage, "OpenMW XR Reference Space Stage");
        VrDebug::setName(mReferenceSpaceView, "OpenMW XR Reference Space Stage");
    }

    PFN_xrVoidFunction OpenXRManagerImpl::xrGetFunction(const std::string& name)
    {
        PFN_xrVoidFunction function = nullptr;
        xrGetInstanceProcAddr(mInstance, name.c_str(), &function);
        return function;
    }

    void OpenXRManagerImpl::enablePredictions()
    {
        mPredictionsEnabled = true;
    }

    void OpenXRManagerImpl::disablePredictions()
    {
        mPredictionsEnabled = false;
    }

    long long OpenXRManagerImpl::getLastPredictedDisplayTime()
    {
        return mFrameState.predictedDisplayTime;
    }

    long long OpenXRManagerImpl::getLastPredictedDisplayPeriod()
    {
        return mFrameState.predictedDisplayPeriod;
    }
    std::array<SwapchainConfig, 2> OpenXRManagerImpl::getRecommendedSwapchainConfig() const
    {
        std::array<SwapchainConfig, 2> config{};
        for (uint32_t i = 0; i < 2; i++)
            config[i] = SwapchainConfig{
                (int)mConfigViews[i].recommendedImageRectWidth,
                (int)mConfigViews[i].recommendedImageRectHeight,
                (int)mConfigViews[i].recommendedSwapchainSampleCount,
                (int)mConfigViews[i].maxImageRectWidth,
                (int)mConfigViews[i].maxImageRectHeight,
                (int)mConfigViews[i].maxSwapchainSampleCount,
        };
        return config;
    }

    osg::Vec3 fromXR(XrVector3f v)
    {
        return osg::Vec3{ v.x, -v.z, v.y };
    }

    osg::Quat fromXR(XrQuaternionf quat)
    {
        return osg::Quat{ quat.x, -quat.z, quat.y, quat.w };
    }

    XrVector3f toXR(osg::Vec3 v)
    {
        return XrVector3f{ v.x(), v.z(), -v.y() };
    }

    XrQuaternionf toXR(osg::Quat quat)
    {
        return XrQuaternionf{ static_cast<float>(quat.x()), static_cast<float>(quat.z()), static_cast<float>(-quat.y()), static_cast<float>(quat.w()) };
    }
}

