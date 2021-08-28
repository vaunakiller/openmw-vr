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

//#include "../mwmechanics/actorutil.hpp"
//
//#include "../mwbase/world.hpp"
//#include "../mwbase/environment.hpp"
//
//#include "../mwworld/class.hpp"
//#include "../mwworld/player.hpp"
//#include "../mwworld/esmstore.hpp"

#include <Windows.h>
#include <objbase.h>

#include <d3d11.h>
#include <dxgi1_2.h>

#include <openxr/openxr_platform.h>
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

    MAKE_TO_STRING_FUNC(XrReferenceSpaceType);
    MAKE_TO_STRING_FUNC(XrViewConfigurationType);
    MAKE_TO_STRING_FUNC(XrEnvironmentBlendMode);
    MAKE_TO_STRING_FUNC(XrSessionState);
    MAKE_TO_STRING_FUNC(XrResult);
    MAKE_TO_STRING_FUNC(XrFormFactor);
    MAKE_TO_STRING_FUNC(XrStructureType);

    Instance* sInstance = nullptr;

    Instance& Instance::instance()
    {
        assert(sInstance);
        return *sInstance;
    }

    Instance::Instance(osg::GraphicsContext* gc)
        : mPlatform(gc)
    {
        if (!sInstance)
            sInstance = this;
        else
            throw std::logic_error("Duplicated XR::Instance singleton");

        mXrInstance = mPlatform.createXrInstance("openmw_vr");

        LogInstanceInfo();

        setupDebugMessenger();

        setupLayerDepth();

        getSystem();

        enumerateViews();

        // TODO: Blend mode
        // setupBlendMode();

        mXrSession = mPlatform.createXrSession(mXrInstance, mSystemId);

        LogReferenceSpaces();

        createReferenceSpaces();

        initTracker();

        getSystemProperties();

        mVRSession = std::make_unique<Session>(mXrSession, mXrInstance, mViewConfigType);
    }

    void Instance::createReferenceSpaces()
    {
        XrReferenceSpaceCreateInfo createInfo{ XR_TYPE_REFERENCE_SPACE_CREATE_INFO };
        createInfo.poseInReferenceSpace.orientation.w = 1.f; // Identity pose
        createInfo.referenceSpaceType = XR_REFERENCE_SPACE_TYPE_VIEW;
        CHECK_XRCMD(xrCreateReferenceSpace(mXrSession, &createInfo, &mReferenceSpaceView));
        createInfo.referenceSpaceType = XR_REFERENCE_SPACE_TYPE_STAGE;
        CHECK_XRCMD(xrCreateReferenceSpace(mXrSession, &createInfo, &mReferenceSpaceStage));
        createInfo.referenceSpaceType = XR_REFERENCE_SPACE_TYPE_LOCAL;
        CHECK_XRCMD(xrCreateReferenceSpace(mXrSession, &createInfo, &mReferenceSpaceLocal));
    }

    void Instance::getSystem()
    {
        XrSystemGetInfo systemInfo{ XR_TYPE_SYSTEM_GET_INFO };
        systemInfo.formFactor = mFormFactor = XR_FORM_FACTOR_HEAD_MOUNTED_DISPLAY;
        auto res = CHECK_XRCMD(xrGetSystem(mXrInstance, &systemInfo, &mSystemId));
        if (!XR_SUCCEEDED(res))
            mPlatform.initFailure(res, mXrInstance);
    }

    void Instance::getSystemProperties()
    {// Read and log graphics properties for the swapchain
        CHECK_XRCMD(xrGetSystemProperties(mXrInstance, mSystemId, &mSystemProperties));

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

    void Instance::setupLayerDepth()
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
            CHECK_XRCMD(createDebugUtilsMessenger(mXrInstance, &createInfo, &mDebugMessenger));
        }
    }

    void
        Instance::LogInstanceInfo() {

        XrInstanceProperties instanceProperties{ XR_TYPE_INSTANCE_PROPERTIES };
        CHECK_XRCMD(xrGetInstanceProperties(mXrInstance, &instanceProperties));
        Log(Debug::Verbose) << "Instance RuntimeName=" << instanceProperties.runtimeName << " RuntimeVersion=" << instanceProperties.runtimeVersion;
    }

    void
        Instance::LogReferenceSpaces() {

        uint32_t spaceCount = 0;
        CHECK_XRCMD(xrEnumerateReferenceSpaces(mXrSession, 0, &spaceCount, nullptr));
        std::vector<XrReferenceSpaceType> spaces(spaceCount);
        CHECK_XRCMD(xrEnumerateReferenceSpaces(mXrSession, spaceCount, &spaceCount, spaces.data()));

        std::stringstream ss;
        ss << "Available reference spaces=" << spaceCount << std::endl;

        for (XrReferenceSpaceType space : spaces)
            ss << "  Name: " << to_string(space) << std::endl;
        Log(Debug::Verbose) << ss.str();
    }

    void
        Instance::endFrame(VR::Frame& frame)
    {
        std::array<XrCompositionLayerProjectionView, 2> compositionLayerProjectionViews{};
        XrCompositionLayerProjection layer{};
        auto* xrLayerStack = reinterpret_cast<XrCompositionLayerBaseHeader*>(&layer);
        std::array<XrCompositionLayerDepthInfoKHR, 2> compositionLayerDepth{};
        XrFrameEndInfo frameEndInfo{ XR_TYPE_FRAME_END_INFO };
        frameEndInfo.displayTime = frame.predictedDisplayTime;
        frameEndInfo.environmentBlendMode = mEnvironmentBlendMode;
        if (frame.shouldRender && frame.layers.size() > 0)
        {
            // For now, hardcode assumption that it's a projection layer
            VR::ProjectionLayer* projectionLayer = static_cast<VR::ProjectionLayer*>(frame.layers[0].get());

            layer.type = XR_TYPE_COMPOSITION_LAYER_PROJECTION;
            layer.space = mReferenceSpaceStage;
            layer.viewCount = 2;
            layer.views = compositionLayerProjectionViews.data();

            bool includeDepth = xrExtensionIsEnabled(XR_KHR_COMPOSITION_LAYER_DEPTH_EXTENSION_NAME);
            auto farClip = Settings::Manager::getFloat("viewing distance", "Camera");

            for (uint32_t i = 0; i < 2; i++)
            {
                auto& xrView = compositionLayerProjectionViews[i];
                auto& view = projectionLayer->views[i];
                xrView.type = XR_TYPE_COMPOSITION_LAYER_PROJECTION_VIEW;
                xrView.fov = toXR(view.view.fov);
                xrView.pose = toXR(view.view.pose);
                xrView.subImage.imageArrayIndex = 0;
                xrView.subImage.imageRect.extent.width = view.subImage.width;
                xrView.subImage.imageRect.extent.height = view.subImage.height;
                xrView.subImage.imageRect.offset.x = view.subImage.x;
                xrView.subImage.imageRect.offset.y = view.subImage.y;
                xrView.subImage.swapchain = static_cast<XrSwapchain>(view.colorSwapchain->handle());

                if (includeDepth && view.depthSwapchain)
                {
                    auto& xrDepth = compositionLayerDepth[i];
                    xrDepth.minDepth = mLayerDepth[i].minDepth;
                    xrDepth.maxDepth = mLayerDepth[i].maxDepth;
                    xrDepth.nearZ = mLayerDepth[i].nearZ;
                    xrDepth.farZ = farClip;
                    xrDepth.subImage.imageArrayIndex = 0;
                    xrDepth.subImage.imageRect.extent.width = view.subImage.width;
                    xrDepth.subImage.imageRect.extent.height = view.subImage.height;
                    xrDepth.subImage.imageRect.offset.x = view.subImage.x;
                    xrDepth.subImage.imageRect.offset.y = view.subImage.y;
                    xrDepth.subImage.swapchain = static_cast<XrSwapchain>(view.depthSwapchain->handle());
                }
            }

            frameEndInfo.layerCount = 1;
            frameEndInfo.layers = &xrLayerStack;
        }
        else
        {
            frameEndInfo.layerCount = 0;
            frameEndInfo.layers = nullptr;
        }
        CHECK_XRCMD(xrEndFrame(mXrSession, &frameEndInfo));
    }

    std::array<Misc::View, 2>
        Instance::getPredictedViews(
            int64_t predictedDisplayTime,
            VR::ReferenceSpace space)
    {
        std::array<XrView, 2> xrViews{ {{XR_TYPE_VIEW}, {XR_TYPE_VIEW}} };
        XrViewState viewState{ XR_TYPE_VIEW_STATE };
        uint32_t viewCount = 2;

        XrViewLocateInfo viewLocateInfo{ XR_TYPE_VIEW_LOCATE_INFO };
        viewLocateInfo.viewConfigurationType = mViewConfigType;
        viewLocateInfo.displayTime = predictedDisplayTime;
        switch (space)
        {
        case VR::ReferenceSpace::Stage:
            viewLocateInfo.space = mReferenceSpaceStage;
            break;
        case VR::ReferenceSpace::View:
            viewLocateInfo.space = mReferenceSpaceView;
            break;
        }
        CHECK_XRCMD(xrLocateViews(mXrSession, &viewLocateInfo, &viewState, viewCount, &viewCount, xrViews.data()));

        std::array<Misc::View, 2> vrViews{};
        for (auto side : { VR::Side_Left, VR::Side_Right })
        {
            vrViews[side].pose = fromXR(xrViews[side].pose);
            vrViews[side].fov = fromXR(xrViews[side].fov);
        }
        return vrViews;
    }

    Misc::Pose Instance::getPredictedHeadPose(
        int64_t predictedDisplayTime,
        VR::ReferenceSpace space)
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
        case VR::ReferenceSpace::Stage:
            referenceSpace = mReferenceSpaceStage;
            break;
        case VR::ReferenceSpace::View:
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
        return Misc::Pose{
            fromXR(location.pose.position),
            fromXR(location.pose.orientation)
        };
    }

    bool Instance::xrExtensionIsEnabled(const char* extensionName) const
    {
        return mPlatform.extensionEnabled(extensionName);
    }

    void Instance::xrUpdateNames()
    {
        Debugging::setName(mXrInstance, "OpenMW XR Instance");
        Debugging::setName(mXrSession, "OpenMW XR Session");
        Debugging::setName(mReferenceSpaceStage, "OpenMW XR Reference Space Stage");
        Debugging::setName(mReferenceSpaceView, "OpenMW XR Reference Space Stage");
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
        return mPlatform.selectColorFormat();
    }

    int64_t Instance::selectDepthFormat()
    {
        // Find supported depth swapchain format.
        return mPlatform.selectDepthFormat();
    }

    void Instance::eraseFormat(int64_t format)
    {
        mPlatform.eraseFormat(format);
    }

    void Instance::initTracker()
    {
        auto stageUserPath = VR::stringToVRPath("/stage/user");
        auto stageUserHeadPath = VR::stringToVRPath("/stage/user/head/input/pose");

        mTracker.reset(new Tracker(stageUserPath, mReferenceSpaceStage));
        mTracker->addTrackingSpace(stageUserHeadPath, mReferenceSpaceView);

        auto worldUserPath = VR::stringToVRPath("/world/user");
        auto worldUserHeadPath = VR::stringToVRPath("/world/user/head/input/pose");
        mTrackerToWorldBinding.reset(new VR::StageToWorldBinding(worldUserPath, mTracker, stageUserHeadPath));
        mTrackerToWorldBinding->bindPaths(worldUserHeadPath, stageUserHeadPath);
    }

    std::vector<uint64_t>
        enumerateSwapchainImagesOpenGL(XrSwapchain swapchain)
    {
        uint32_t imageCount = 0;
        std::vector<uint64_t> images;
        std::vector<XrSwapchainImageOpenGLKHR> xrimages;
        CHECK_XRCMD(xrEnumerateSwapchainImages(swapchain, 0, &imageCount, nullptr));
        xrimages.resize(imageCount, { XR_TYPE_SWAPCHAIN_IMAGE_OPENGL_KHR });
        CHECK_XRCMD(xrEnumerateSwapchainImages(swapchain, imageCount, &imageCount, reinterpret_cast<XrSwapchainImageBaseHeader*>(xrimages.data())));

        for (auto& image : xrimages)
        {
            images.push_back(image.image);
        }

        return images;
    }

    std::vector<uint64_t>
        enumerateSwapchainImagesDirectX(XrSwapchain swapchain)
    {
        uint32_t imageCount = 0;
        std::vector<uint64_t> images;
        std::vector<XrSwapchainImageD3D11KHR> xrimages;
        CHECK_XRCMD(xrEnumerateSwapchainImages(swapchain, 0, &imageCount, nullptr));
        xrimages.resize(imageCount, { XR_TYPE_SWAPCHAIN_IMAGE_D3D11_KHR });
        CHECK_XRCMD(xrEnumerateSwapchainImages(swapchain, imageCount, &imageCount, reinterpret_cast<XrSwapchainImageBaseHeader*>(xrimages.data())));

        for (auto& image : xrimages)
        {
            images.push_back(reinterpret_cast<uint64_t>(image.texture));
        }

        return images;
    }

    VR::Swapchain* Instance::createSwapchain(uint32_t width, uint32_t height, uint32_t samples, SwapchainUse use, const std::string& name)
    {
        std::string typeString = use == SwapchainUse::Color ? "color" : "depth";

        XrSwapchainCreateInfo swapchainCreateInfo{ XR_TYPE_SWAPCHAIN_CREATE_INFO };
        swapchainCreateInfo.arraySize = 1;
        swapchainCreateInfo.width = width;
        swapchainCreateInfo.height = height;
        swapchainCreateInfo.mipCount = 1;
        swapchainCreateInfo.faceCount = 1;
        if (use == SwapchainUse::Color)
            swapchainCreateInfo.usageFlags = XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT;
        else
            swapchainCreateInfo.usageFlags = XR_SWAPCHAIN_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;

        XrSwapchain swapchain = XR_NULL_HANDLE;
        int format = 0;

        while (samples > 0 && swapchain == XR_NULL_HANDLE && format == 0)
        {
            // Select a swapchain format.
            if (use == SwapchainUse::Color)
                format = selectColorFormat();
            else
                format = selectDepthFormat();
            if (format == 0) {
                throw std::runtime_error(std::string("Swapchain ") + typeString + " format not supported");
            }
            Log(Debug::Verbose) << "Selected " << typeString << " format: " << std::dec << format << " (" << std::hex << format << ")" << std::dec;

            // Now create the swapchain
            Log(Debug::Verbose) << "Creating swapchain with dimensions Width=" << width << " Heigh=" << height << " SampleCount=" << samples;
            swapchainCreateInfo.format = format;
            swapchainCreateInfo.sampleCount = samples;
            auto res = xrCreateSwapchain(xrSession(), &swapchainCreateInfo, &swapchain);

            // Check errors and try again if possible
            if (res == XR_ERROR_SWAPCHAIN_FORMAT_UNSUPPORTED)
            {
                // We only try swapchain formats enumerated by the runtime itself.
                // This does not guarantee that that swapchain format is going to be supported for this specific usage.
                Log(Debug::Verbose) << "Failed to create swapchain with Format=" << format << ": " << XrResultString(res);
                eraseFormat(format);
                format = 0;
                continue;
            }
            else if (!XR_SUCCEEDED(res))
            {
                Log(Debug::Verbose) << "Failed to create swapchain with SampleCount=" << samples << ": " << XrResultString(res);
                samples /= 2;
                if (samples == 0)
                {
                    CHECK_XRRESULT(res, "xrCreateSwapchain");
                    throw std::runtime_error(XrResultString(res));
                }
                continue;
            }

            CHECK_XRRESULT(res, "xrCreateSwapchain");
            if (use == SwapchainUse::Color)
                Debugging::setName(swapchain, "OpenMW XR Color Swapchain " + name);
            else
                Debugging::setName(swapchain, "OpenMW XR Depth Swapchain " + name);

            if (xrExtensionIsEnabled(XR_KHR_D3D11_ENABLE_EXTENSION_NAME))
            {
                auto images = enumerateSwapchainImagesDirectX(swapchain);
                return new VR::DirectXSwapchain(std::make_shared<Swapchain>(swapchain, images, width, height, samples, format), mPlatform.dxInterop());
            }
            else if (xrExtensionIsEnabled(XR_KHR_OPENGL_ENABLE_EXTENSION_NAME))
            {
                auto images = enumerateSwapchainImagesOpenGL(swapchain);
                return new Swapchain(swapchain, images, width, height, samples, format);
            }
            else
            {
                throw std::logic_error("Not implemented");
            }
        }

        // Never actually reached
        return nullptr;
    }

    void Instance::enablePredictions()
    {
        mPredictionsEnabled = true;
    }

    void Instance::disablePredictions()
    {
        mPredictionsEnabled = false;
    }

    long long Instance::getLastPredictedDisplayTime()
    {
        return mFrameState.predictedDisplayTime;
    }

    long long Instance::getLastPredictedDisplayPeriod()
    {
        return mFrameState.predictedDisplayPeriod;
    }
    std::array<SwapchainConfig, 2> Instance::getRecommendedSwapchainConfig() const
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
    XrSpace Instance::getReferenceSpace(VR::ReferenceSpace space)
    {
        switch (space)
        {
        case VR::ReferenceSpace::Stage:
            return mReferenceSpaceStage;
        case VR::ReferenceSpace::View:
            return mReferenceSpaceView;
        }
        return XR_NULL_HANDLE;
    }
}

