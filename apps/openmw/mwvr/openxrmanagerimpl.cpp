#include "openxrmanagerimpl.hpp"
#include "openxrdebug.hpp"
#include "openxrplatform.hpp"
#include "openxrswapchain.hpp"
#include "openxrtypeconversions.hpp"
#include "vrenvironment.hpp"
#include "vrinputmanager.hpp"

#include <components/debug/debuglog.hpp>
#include <components/sdlutil/sdlgraphicswindow.hpp>
#include <components/esm/loadrace.hpp>
#include <components/vr/directx.hpp>
#include <components/vr/layer.hpp>

#include "../mwmechanics/actorutil.hpp"

#include "../mwbase/world.hpp"
#include "../mwbase/environment.hpp"

#include "../mwworld/class.hpp"
#include "../mwworld/player.hpp"
#include "../mwworld/esmstore.hpp"

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
    OpenXRManagerImpl::OpenXRManagerImpl(osg::GraphicsContext* gc)
        : mPlatform(gc)
    {
        mInstance = mPlatform.createXrInstance("openmw_vr");

        LogInstanceInfo();

        setupDebugMessenger();

        setupLayerDepth();

        getSystem();

        enumerateViews();

        // TODO: Blend mode
        // setupBlendMode();

        mSession = mPlatform.createXrSession(mInstance, mSystemId);
        
        LogReferenceSpaces();

        createReferenceSpaces();

        initTracker();

        getSystemProperties();
    }

    void OpenXRManagerImpl::createReferenceSpaces()
    {
        XrReferenceSpaceCreateInfo createInfo{ XR_TYPE_REFERENCE_SPACE_CREATE_INFO };
        createInfo.poseInReferenceSpace.orientation.w = 1.f; // Identity pose
        createInfo.referenceSpaceType = XR_REFERENCE_SPACE_TYPE_VIEW;
        CHECK_XRCMD(xrCreateReferenceSpace(mSession, &createInfo, &mReferenceSpaceView));
        createInfo.referenceSpaceType = XR_REFERENCE_SPACE_TYPE_STAGE;
        CHECK_XRCMD(xrCreateReferenceSpace(mSession, &createInfo, &mReferenceSpaceStage));
        createInfo.referenceSpaceType = XR_REFERENCE_SPACE_TYPE_LOCAL;
        CHECK_XRCMD(xrCreateReferenceSpace(mSession, &createInfo, &mReferenceSpaceLocal));
    }

    void OpenXRManagerImpl::getSystem()
    {
        XrSystemGetInfo systemInfo{ XR_TYPE_SYSTEM_GET_INFO };
        systemInfo.formFactor = mFormFactor = XR_FORM_FACTOR_HEAD_MOUNTED_DISPLAY;
        auto res = CHECK_XRCMD(xrGetSystem(mInstance, &systemInfo, &mSystemId));
        if (!XR_SUCCEEDED(res))
            mPlatform.initFailure(res, mInstance);
    }

    void OpenXRManagerImpl::getSystemProperties()
    {// Read and log graphics properties for the swapchain
        CHECK_XRCMD(xrGetSystemProperties(mInstance, mSystemId, &mSystemProperties));

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

    void OpenXRManagerImpl::enumerateViews()
    {
        uint32_t viewCount = 0;
        CHECK_XRCMD(xrEnumerateViewConfigurationViews(mInstance, mSystemId, mViewConfigType, 2, &viewCount, mConfigViews.data()));

        if (viewCount != 2)
        {
            std::stringstream ss;
            ss << "xrEnumerateViewConfigurationViews returned " << viewCount << " views";
            Log(Debug::Verbose) << ss.str();
        }
    }

    void OpenXRManagerImpl::setupLayerDepth()
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

    OpenXRManagerImpl::~OpenXRManagerImpl()
    {

    }

    void OpenXRManagerImpl::setupExtensionsAndLayers()
    {

    }
    static XrBool32 xrDebugCallback(
        XrDebugUtilsMessageSeverityFlagsEXT messageSeverity,
        XrDebugUtilsMessageTypeFlagsEXT messageType,
        const XrDebugUtilsMessengerCallbackDataEXT* callbackData,
        void* userData)
    {
        OpenXRManagerImpl* manager = reinterpret_cast<OpenXRManagerImpl*>(userData);
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
        OpenXRManagerImpl::LogInstanceInfo() {

        XrInstanceProperties instanceProperties{ XR_TYPE_INSTANCE_PROPERTIES };
        CHECK_XRCMD(xrGetInstanceProperties(mInstance, &instanceProperties));
        Log(Debug::Verbose) << "Instance RuntimeName=" << instanceProperties.runtimeName << " RuntimeVersion=" << instanceProperties.runtimeVersion;
    }

    void
        OpenXRManagerImpl::LogReferenceSpaces() {

        uint32_t spaceCount = 0;
        CHECK_XRCMD(xrEnumerateReferenceSpaces(mSession, 0, &spaceCount, nullptr));
        std::vector<XrReferenceSpaceType> spaces(spaceCount);
        CHECK_XRCMD(xrEnumerateReferenceSpaces(mSession, spaceCount, &spaceCount, spaces.data()));

        std::stringstream ss;
        ss << "Available reference spaces=" << spaceCount << std::endl;

        for (XrReferenceSpaceType space : spaces)
            ss << "  Name: " << to_string(space) << std::endl;
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

    void
        OpenXRManagerImpl::beginFrame()
    {
        XrFrameBeginInfo frameBeginInfo{ XR_TYPE_FRAME_BEGIN_INFO };
        CHECK_XRCMD(xrBeginFrame(mSession, &frameBeginInfo));
    }

    void
        OpenXRManagerImpl::endFrame(VR::Frame& frame)
    {
        std::array<XrCompositionLayerProjectionView, 2> compositionLayerProjectionViews{};
        XrCompositionLayerProjection layer{};
        auto* xrLayerStack = reinterpret_cast<XrCompositionLayerBaseHeader*>(&layer);
        std::array<XrCompositionLayerDepthInfoKHR, 2> compositionLayerDepth{};
        XrFrameEndInfo frameEndInfo{ XR_TYPE_FRAME_END_INFO };
        frameEndInfo.displayTime = frame.runtimePredictedDisplayTime;
        frameEndInfo.environmentBlendMode = mEnvironmentBlendMode;
        if (frame.shouldRender && frame.layers.size() > 0)
        {
            // For now, hardcode assumption that it's a projection layer
            VR::ProjectionLayer* projectionLayer = static_cast<VR::ProjectionLayer*>(frame.layers[0].get());

            //compositionLayerProjectionViews[(int)Side::LEFT_SIDE] = toXR(projectionLayer->mViews[(int)Side::LEFT_SIDE]);
            //compositionLayerProjectionViews[(int)Side::RIGHT_SIDE] = toXR(projectionLayer->mViews[(int)Side::RIGHT_SIDE]);
            layer.type = XR_TYPE_COMPOSITION_LAYER_PROJECTION;
            layer.space = mReferenceSpaceStage;
            layer.viewCount = 2;
            layer.views = compositionLayerProjectionViews.data();

            //if (xrExtensionIsEnabled(XR_KHR_COMPOSITION_LAYER_DEPTH_EXTENSION_NAME))
            //{
            //    auto farClip = Settings::Manager::getFloat("viewing distance", "Camera");
            //    // All values not set here are set previously as they are constant
            //    compositionLayerDepth = mLayerDepth;
            //    compositionLayerDepth[(int)Side::LEFT_SIDE].farZ = farClip;
            //    compositionLayerDepth[(int)Side::RIGHT_SIDE].farZ = farClip;
            //    compositionLayerDepth[(int)Side::LEFT_SIDE].subImage = toXR((*layerStack)[(int)Side::LEFT_SIDE].subImage, true);
            //    compositionLayerDepth[(int)Side::RIGHT_SIDE].subImage = toXR((*layerStack)[(int)Side::RIGHT_SIDE].subImage, true);
            //    if (compositionLayerDepth[(int)Side::LEFT_SIDE].subImage.swapchain != XR_NULL_HANDLE
            //        && compositionLayerDepth[(int)Side::RIGHT_SIDE].subImage.swapchain != XR_NULL_HANDLE)
            //    {
            //        compositionLayerProjectionViews[(int)Side::LEFT_SIDE].next = &compositionLayerDepth[(int)Side::LEFT_SIDE];
            //        compositionLayerProjectionViews[(int)Side::RIGHT_SIDE].next = &compositionLayerDepth[(int)Side::RIGHT_SIDE];
            //    }
            //}

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
        CHECK_XRCMD(xrEndFrame(mSession, &frameEndInfo));
    }

    std::array<View, 2>
        OpenXRManagerImpl::getPredictedViews(
            int64_t predictedDisplayTime,
            ReferenceSpace space)
    {
        //if (!mPredictionsEnabled)
        //{
        //    Log(Debug::Error) << "Prediction out of order";
        //    throw std::logic_error("Prediction out of order");
        //}
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
        case XR_TYPE_EVENT_DATA_INTERACTION_PROFILE_CHANGED:
            MWVR::Environment::get().getInputManager()->notifyInteractionProfileChanged();
            break;
        case XR_TYPE_EVENT_DATA_INSTANCE_LOSS_PENDING:
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

    bool OpenXRManagerImpl::xrExtensionIsEnabled(const char* extensionName) const
    {
        return mPlatform.extensionEnabled(extensionName);
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

    int64_t OpenXRManagerImpl::selectColorFormat()
    {
        // Find supported color swapchain format.
        return mPlatform.selectColorFormat();
    }

    int64_t OpenXRManagerImpl::selectDepthFormat()
    {
        // Find supported depth swapchain format.
        return mPlatform.selectDepthFormat();
    }

    void OpenXRManagerImpl::eraseFormat(int64_t format)
    {
        mPlatform.eraseFormat(format);
    }

    void OpenXRManagerImpl::initTracker()
    {
        auto* trackingManager = Environment::get().getTrackingManager();
        auto stageUserPath = trackingManager->stringToVRPath("/stage/user");
        auto stageUserHeadPath = trackingManager->stringToVRPath("/stage/user/head/input/pose");

        mTracker.reset(new OpenXRTracker(stageUserPath, mReferenceSpaceStage));
        mTracker->addTrackingSpace(stageUserHeadPath, mReferenceSpaceView);

        auto worldUserPath = trackingManager->stringToVRPath("/world/user");
        auto worldUserHeadPath = trackingManager->stringToVRPath("/world/user/head/input/pose");
        mTrackerToWorldBinding.reset(new VRStageToWorldBinding(worldUserPath, mTracker, stageUserHeadPath));
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
        xrimages.resize(imageCount, { XR_TYPE_SWAPCHAIN_IMAGE_D3D11_KHR});
        CHECK_XRCMD(xrEnumerateSwapchainImages(swapchain, imageCount, &imageCount, reinterpret_cast<XrSwapchainImageBaseHeader*>(xrimages.data())));

        for (auto& image : xrimages)
        {
            images.push_back(reinterpret_cast<uint64_t>(image.texture));
        }

        return images;
    }

    VR::Swapchain* OpenXRManagerImpl::createSwapchain(uint32_t width, uint32_t height, uint32_t samples, SwapchainUse use, const std::string& name)
    {
        auto* xr = Environment::get().getManager();
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
                format = xr->selectColorFormat();
            else
                format = xr->selectDepthFormat();
            if (format == 0) {
                throw std::runtime_error(std::string("Swapchain ") + typeString + " format not supported");
            }
            Log(Debug::Verbose) << "Selected " << typeString << " format: " << std::dec << format << " (" << std::hex << format << ")" << std::dec;

            // Now create the swapchain
            Log(Debug::Verbose) << "Creating swapchain with dimensions Width=" << width << " Heigh=" << height << " SampleCount=" << samples;
            swapchainCreateInfo.format = format;
            swapchainCreateInfo.sampleCount = samples;
            auto res = xrCreateSwapchain(xr->impl().xrSession(), &swapchainCreateInfo, &swapchain);

            // Check errors and try again if possible
            if (res == XR_ERROR_SWAPCHAIN_FORMAT_UNSUPPORTED)
            {
                // We only try swapchain formats enumerated by the runtime itself.
                // This does not guarantee that that swapchain format is going to be supported for this specific usage.
                Log(Debug::Verbose) << "Failed to create swapchain with Format=" << format << ": " << XrResultString(res);
                xr->eraseFormat(format);
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
                VrDebug::setName(swapchain, "OpenMW XR Color Swapchain " + name);
            else
                VrDebug::setName(swapchain, "OpenMW XR Depth Swapchain " + name);

            if (xrExtensionIsEnabled(XR_KHR_D3D11_ENABLE_EXTENSION_NAME))
            {
                auto images = enumerateSwapchainImagesDirectX(swapchain);
                return new VR::DirectXSwapchain(std::make_shared<OpenXRSwapchain>(swapchain, images, width, height, samples, format), mPlatform.dxInterop());
            }
            else if (xrExtensionIsEnabled(XR_KHR_OPENGL_ENABLE_EXTENSION_NAME))
            {
                auto images = enumerateSwapchainImagesOpenGL(swapchain);
                return new OpenXRSwapchain(swapchain, images, width, height, samples, format);
            }
            else
            {
                throw std::logic_error("Not implemented");
            }
        }

        // Never actually reached
        return nullptr;
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
    XrSpace OpenXRManagerImpl::getReferenceSpace(ReferenceSpace space)
    {
        switch (space)
        {
        case ReferenceSpace::STAGE:
            return mReferenceSpaceStage;
        case ReferenceSpace::VIEW:
            return mReferenceSpaceView;
        }
        return XR_NULL_HANDLE;
    }
}

