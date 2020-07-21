#include "openxrmanagerimpl.hpp"

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

// The OpenXR SDK assumes we've included Windows.h
#include <Windows.h>

#include <openxr/openxr.h>
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

#if !XR_KHR_composition_layer_depth || !XR_KHR_opengl_enable
#error "OpenXR extensions missing. Please upgrade your copy of the OpenXR SDK"
#endif

namespace MWVR
{
    OpenXRManagerImpl::OpenXRManagerImpl()
    {
        
        std::vector<const char*> extensions = {
            XR_KHR_OPENGL_ENABLE_EXTENSION_NAME,
            XR_KHR_COMPOSITION_LAYER_DEPTH_EXTENSION_NAME,
        };

        { // Create Instance
            XrInstanceCreateInfo createInfo{ XR_TYPE_INSTANCE_CREATE_INFO };
            createInfo.next = nullptr;
            createInfo.enabledExtensionCount = extensions.size();
            createInfo.enabledExtensionNames = extensions.data();
            strcpy(createInfo.applicationInfo.applicationName, "openmw_vr");
            createInfo.applicationInfo.apiVersion = XR_CURRENT_API_VERSION;
            // Iteratively strip extensions until instance creation succeeds.
            XrResult result = xrCreateInstance(&createInfo, &mInstance);
            while (result == XR_ERROR_EXTENSION_NOT_PRESENT)
            {
                createInfo.enabledExtensionCount--;
                result = xrCreateInstance(&createInfo, &mInstance);
            }

            mEnabledExtensions.insert(extensions.begin(), extensions.begin() + createInfo.enabledExtensionCount);

            if (!xrExtensionIsEnabled(XR_KHR_OPENGL_ENABLE_EXTENSION_NAME))
                throw std::runtime_error(std::string("Required OpenXR extension ") + XR_KHR_OPENGL_ENABLE_EXTENSION_NAME + " not supported");

            Log(Debug::Verbose) << "OpenXR Extension status:";
            for (auto* ext : extensions)
            {
                if (!xrExtensionIsEnabled(ext))
                {
                    Log(Debug::Verbose) << "  " << ext << ": disabled (not supported)";
                }
                else
                {
                    Log(Debug::Verbose) << "  " << ext << ": enabled";
                }
            }


            assert(mInstance);
        }

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

        { // Create Session
            // TODO: Platform dependent
            auto DC = wglGetCurrentDC();
            auto GLRC = wglGetCurrentContext();
            auto XRGLRC = wglCreateContext(DC);
            wglShareLists(GLRC, XRGLRC);
            wglMakeCurrent(DC, XRGLRC);

            mGraphicsBinding.type = XR_TYPE_GRAPHICS_BINDING_OPENGL_WIN32_KHR;
            mGraphicsBinding.next = nullptr;
            mGraphicsBinding.hDC = DC;
            mGraphicsBinding.hGLRC = XRGLRC;

            if (!mGraphicsBinding.hDC)
                std::cout << "Missing DC" << std::endl;
            if (!mGraphicsBinding.hGLRC)
                std::cout << "Missing GLRC" << std::endl;

            XrSessionCreateInfo createInfo{ XR_TYPE_SESSION_CREATE_INFO };
            createInfo.next = &mGraphicsBinding;
            createInfo.systemId = mSystemId;
            CHECK_XRCMD(xrCreateSession(mInstance, &createInfo, &mSession));
            assert(mSession);
            wglMakeCurrent(DC, GLRC);
        }

        LogLayersAndExtensions();
        LogInstanceInfo();
        LogReferenceSpaces();

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

            // OpenXR gives me crazy bananas high resolutions. Likely an oculus bug.
            mConfigViews[0].recommendedImageRectHeight = 1200;
            mConfigViews[1].recommendedImageRectHeight = 1200;
            mConfigViews[0].recommendedImageRectWidth = 1080;
            mConfigViews[1].recommendedImageRectWidth = 1080;

            if (viewCount != 2)
            {
                std::stringstream ss;
                ss << "xrEnumerateViewConfigurationViews returned " << viewCount << " views";
                Log(Debug::Verbose) << ss.str();
            }
        }
    }

    inline XrResult CheckXrResult(XrResult res, const char* originator, const char* sourceLocation) {
        if (XR_FAILED(res)) {
            std::stringstream ss;
            ss << sourceLocation << ": OpenXR[Error: " << to_string(res) << "][Thread: " << std::this_thread::get_id() << "][DC: " << wglGetCurrentDC() << "][GLRC: " << wglGetCurrentContext() << "]: " << originator;
            Log(Debug::Error) << ss.str();
            throw std::runtime_error(ss.str().c_str());
        }
        else
        {
            // Log(Debug::Verbose) << sourceLocation << ": OpenXR[" << to_string(res) << "][" << std::this_thread::get_id() << "][" << wglGetCurrentDC() << "][" << wglGetCurrentContext() << "]: " << originator;
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



    void
        OpenXRManagerImpl::LogLayersAndExtensions() {
        // Write out extension properties for a given layer.
        const auto logExtensions = [](const char* layerName, int indent = 0) {
            uint32_t instanceExtensionCount;
            CHECK_XRCMD(xrEnumerateInstanceExtensionProperties(layerName, 0, &instanceExtensionCount, nullptr));

            std::vector<XrExtensionProperties> extensions(instanceExtensionCount);
            for (XrExtensionProperties& extension : extensions) {
                extension.type = XR_TYPE_EXTENSION_PROPERTIES;
            }

            CHECK_XRCMD(xrEnumerateInstanceExtensionProperties(layerName, (uint32_t)extensions.size(), &instanceExtensionCount,
                extensions.data()));

            const std::string indentStr(indent, ' ');

            std::stringstream ss;
            ss << indentStr.c_str() << "Available Extensions: (" << instanceExtensionCount << ")" << std::endl;


            for (const XrExtensionProperties& extension : extensions) {
                ss << indentStr << "  Name=" << std::string(extension.extensionName) << " SpecVersion=" << extension.extensionVersion << std::endl;
            }

            Log(Debug::Verbose) << ss.str();
        };

        // Log non-layer extensions (layerName==nullptr).
        logExtensions(nullptr);

        // Log layers and any of their extensions.
        {
            uint32_t layerCount;
            CHECK_XRCMD(xrEnumerateApiLayerProperties(0, &layerCount, nullptr));

            std::vector<XrApiLayerProperties> layers(layerCount);
            for (XrApiLayerProperties& layer : layers) {
                layer.type = XR_TYPE_API_LAYER_PROPERTIES;
            }

            CHECK_XRCMD(xrEnumerateApiLayerProperties((uint32_t)layers.size(), &layerCount, layers.data()));

            std::stringstream ss;
            ss << "Available Layers: (" << layerCount << ")" << std::endl;

            for (const XrApiLayerProperties& layer : layers) {
                ss << "  Name=" << layer.layerName << " SpecVersion=" << layer.layerVersion << std::endl;
                logExtensions(layer.layerName, 2);
            }

            Log(Debug::Verbose) << ss.str();
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

    void
        OpenXRManagerImpl::waitFrame()
    {
        XrFrameWaitInfo frameWaitInfo{ XR_TYPE_FRAME_WAIT_INFO };
        XrFrameState frameState{ XR_TYPE_FRAME_STATE };

        CHECK_XRCMD(xrWaitFrame(mSession, &frameWaitInfo, &frameState));
        mFrameState = frameState;
    }

    void
        OpenXRManagerImpl::beginFrame()
    {
        auto DC = wglGetCurrentDC();
        auto GLRC = wglGetCurrentContext();
        auto XRDC = mGraphicsBinding.hDC;
        auto XRGLRC = mGraphicsBinding.hGLRC;
        wglMakeCurrent(XRDC, XRGLRC);

        XrFrameBeginInfo frameBeginInfo{ XR_TYPE_FRAME_BEGIN_INFO };
        CHECK_XRCMD(xrBeginFrame(mSession, &frameBeginInfo));

        wglMakeCurrent(DC, GLRC);
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
        OpenXRManagerImpl::endFrame(int64_t displayTime, int layerCount, const std::array<CompositionLayerProjectionView, 2>& layerStack)
    {
        auto DC = wglGetCurrentDC();
        auto GLRC = wglGetCurrentContext();
        auto XRDC = mGraphicsBinding.hDC;
        auto XRGLRC = mGraphicsBinding.hGLRC;
        wglMakeCurrent(XRDC, XRGLRC);

        std::array<XrCompositionLayerProjectionView, 2> compositionLayerProjectionViews{};
        compositionLayerProjectionViews[(int)Side::LEFT_SIDE] = toXR(layerStack[(int)Side::LEFT_SIDE]);
        compositionLayerProjectionViews[(int)Side::RIGHT_SIDE] = toXR(layerStack[(int)Side::RIGHT_SIDE]);
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
            compositionLayerDepth[(int)Side::LEFT_SIDE].farZ = farClip;
            compositionLayerDepth[(int)Side::RIGHT_SIDE].farZ = farClip;
            compositionLayerDepth[(int)Side::LEFT_SIDE].subImage = layerStack[(int)Side::LEFT_SIDE].swapchain->impl().xrSubImageDepth();
            compositionLayerDepth[(int)Side::RIGHT_SIDE].subImage = layerStack[(int)Side::RIGHT_SIDE].swapchain->impl().xrSubImageDepth();
            compositionLayerProjectionViews[(int)Side::LEFT_SIDE].next = &compositionLayerDepth[(int)Side::LEFT_SIDE];
            compositionLayerProjectionViews[(int)Side::RIGHT_SIDE].next = &compositionLayerDepth[(int)Side::RIGHT_SIDE];
        }

        XrFrameEndInfo frameEndInfo{ XR_TYPE_FRAME_END_INFO };
        frameEndInfo.displayTime = displayTime;
        frameEndInfo.environmentBlendMode = mEnvironmentBlendMode;
        frameEndInfo.layerCount = layerCount;
        frameEndInfo.layers = &xrLayerStack;
        CHECK_XRCMD(xrEndFrame(mSession, &frameEndInfo));

        wglMakeCurrent(DC, GLRC);
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
        std::unique_lock<std::mutex> lock(mEventMutex);

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
        auto oldState = mSessionState;
        auto newState = stateChangedEvent.state;
        Log(Debug::Verbose) << "XrEventDataSessionStateChanged: state " << to_string(oldState) << "->" << to_string(newState);
        bool success = true;

        switch (newState)
        {
        case XR_SESSION_STATE_READY:
        {
            XrSessionBeginInfo beginInfo{ XR_TYPE_SESSION_BEGIN_INFO };
            beginInfo.primaryViewConfigurationType = mViewConfigType;
            CHECK_XRCMD(xrBeginSession(mSession, &beginInfo));
            mSessionRunning = true;
            break;
        }
        case XR_SESSION_STATE_STOPPING:
        {
            if (checkStopCondition())
            {
                CHECK_XRCMD(xrEndSession(mSession));
                mSessionStopRequested = false;
                mSessionRunning = false;
            }
            else
            {
                mSessionStopRequested = true;
                success = false;
            }
            break;
        }
        default:
            Log(Debug::Verbose) << "XrEventDataSessionStateChanged: Ignoring new state " << to_string(newState);
        }

        if (success)
        {
            mSessionState = newState;
        }
        else
        {
            Log(Debug::Verbose) << "XrEventDataSessionStateChanged: Conditions for state " << to_string(newState) << " not met, retrying next frame";
        }

        return success;
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
        return nullptr;
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

    bool OpenXRManagerImpl::frameShouldRender()
    {
        return xrSessionRunning() && !xrSessionStopRequested();
    }

    void OpenXRManagerImpl::xrResourceAcquired()
    {
        mAcquiredResources++;
    }

    void OpenXRManagerImpl::xrResourceReleased()
    {
        mAcquiredResources--;
    }

    bool OpenXRManagerImpl::xrSessionStopRequested()
    {
        return mSessionStopRequested;
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
                mConfigViews[i].recommendedImageRectWidth,
                mConfigViews[i].maxImageRectWidth,
                mConfigViews[i].recommendedImageRectHeight,
                mConfigViews[i].maxImageRectHeight,
                mConfigViews[i].recommendedSwapchainSampleCount,
                mConfigViews[i].recommendedSwapchainSampleCount,
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
        return XrQuaternionf{ quat.x(), quat.z(), -quat.y(), quat.w() };
    }
}

