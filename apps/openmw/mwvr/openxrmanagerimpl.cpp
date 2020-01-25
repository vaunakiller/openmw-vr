#include "openxrmanagerimpl.hpp"
#include "openxrtexture.hpp"

#include <components/debug/debuglog.hpp>
#include <components/sdlutil/sdlgraphicswindow.hpp>

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


namespace MWVR
{
    OpenXRManagerImpl::OpenXRManagerImpl()
    {
        std::vector<const char*> extensions = {
          XR_KHR_OPENGL_ENABLE_EXTENSION_NAME
        };

        { // Create Instance
            XrInstanceCreateInfo createInfo{ XR_TYPE_INSTANCE_CREATE_INFO };
            createInfo.next = nullptr;
            createInfo.enabledExtensionCount = extensions.size();
            createInfo.enabledExtensionNames = extensions.data();

            strcpy(createInfo.applicationInfo.applicationName, "Boo");
            createInfo.applicationInfo.apiVersion = XR_CURRENT_API_VERSION;
            CHECK_XRCMD(xrCreateInstance(&createInfo, &mInstance));
            assert(mInstance);
        }

        { // Get system ID
            XrSystemGetInfo systemInfo{ XR_TYPE_SYSTEM_GET_INFO };
            systemInfo.formFactor = mFormFactor = XR_FORM_FACTOR_HEAD_MOUNTED_DISPLAY;
            CHECK_XRCMD(xrGetSystem(mInstance, &systemInfo, &mSystemId));
            assert(mSystemId);
        }

        { // Initialize OpenGL device

          // This doesn't appear to be intended to do anything of consequence, only return requirements to me. But xrCreateSession fails if xrGetOpenGLGraphicsRequirementsKHR is not called.
          // Oculus Bug?
            PFN_xrGetOpenGLGraphicsRequirementsKHR p_getRequirements = nullptr;
            xrGetInstanceProcAddr(mInstance, "xrGetOpenGLGraphicsRequirementsKHR", reinterpret_cast<PFN_xrVoidFunction*>(&p_getRequirements));
            XrGraphicsRequirementsOpenGLKHR requirements{ XR_TYPE_GRAPHICS_REQUIREMENTS_OPENGL_KHR };
            CHECK_XRCMD(p_getRequirements(mInstance, mSystemId, &requirements));

            //GLint major = 0;
            //GLint minor = 0;
            //glGetIntegerv(GL_MAJOR_VERSION, &major);
            //glGetIntegerv(GL_MINOR_VERSION, &minor);

            const XrVersion desiredApiVersion = XR_MAKE_VERSION(4, 6, 0);
            if (requirements.minApiVersionSupported > desiredApiVersion) {
                std::cout << "Runtime does not support desired Graphics API and/or version" << std::endl;
            }
        }

        { // Create Session
            // TODO: Platform dependent
            mGraphicsBinding.type = XR_TYPE_GRAPHICS_BINDING_OPENGL_WIN32_KHR;
            mGraphicsBinding.next = nullptr;
            mGraphicsBinding.hDC = wglGetCurrentDC();
            mGraphicsBinding.hGLRC = wglGetCurrentContext();

            if (!mGraphicsBinding.hDC)
                std::cout << "Missing DC" << std::endl;
            if (!mGraphicsBinding.hGLRC)
                std::cout << "Missing GLRC" << std::endl;

            XrSessionCreateInfo createInfo{ XR_TYPE_SESSION_CREATE_INFO };
            createInfo.next = &mGraphicsBinding;
            createInfo.systemId = mSystemId;
            createInfo.createFlags;
            CHECK_XRCMD(xrCreateSession(mInstance, &createInfo, &mSession));
            assert(mSession);
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

        { // Set up layers

            //mLayer.space = mReferenceSpaceStage;
            //mLayer.viewCount = (uint32_t)mProjectionLayerViews.size();
            //mLayer.views = mProjectionLayerViews.data();
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
            
            // TODO: This, including the projection layer views, should be moved to openxrviewer
            //for (unsigned i = 0; i < 2; i++)
            //{
            //    mEyes[i].reset(new OpenXRView(this, mConfigViews[i]));

            //    mProjectionLayerViews[i].subImage.swapchain = mEyes[i]->mSwapchain;
            //    mProjectionLayerViews[i].subImage.imageRect.offset = { 0, 0 };
            //    mProjectionLayerViews[i].subImage.imageRect.extent = { mEyes[i]->mWidth, mEyes[i]->mHeight };
            //}
        }
    }

    inline XrResult CheckXrResult(XrResult res, const char* originator, const char* sourceLocation) {
        if (XR_FAILED(res)) {
            Log(Debug::Error) << sourceLocation << ": OpenXR[" << to_string(res) << "]: " << originator;
        }
        else
        {
            Log(Debug::Verbose) << sourceLocation << ": OpenXR[" << to_string(res) << "][" << std::this_thread::get_id() << "][" << wglGetCurrentDC() << "][" << wglGetCurrentContext() << "]: " << originator;
        }

        return res;
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

        std::stringstream ss;
        ss << "Instance RuntimeName=" << instanceProperties.runtimeName << " RuntimeVersion=" << instanceProperties.runtimeVersion;
        Log(Debug::Verbose) << ss.str();
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
        if (!mSessionRunning)
            return;

        XrFrameWaitInfo frameWaitInfo{ XR_TYPE_FRAME_WAIT_INFO };
        XrFrameState frameState{ XR_TYPE_FRAME_STATE };
        CHECK_XRCMD(xrWaitFrame(mSession, &frameWaitInfo, &frameState));
        mFrameState = frameState;
    }

    void
        OpenXRManagerImpl::beginFrame(long long frameIndex)
    {
        Log(Debug::Verbose) << "frameIndex = " << frameIndex;
        if (!mSessionRunning)
            return;

        std::unique_lock<std::mutex> lock(mFrameStatusMutex);

        // We need to wait for the frame to become idle or ready
        // (There is no guarantee osg won't get us here before endFrame() returns)
        while (mFrameStatus == OPENXR_FRAME_STATUS_ENDING || mFrameIndex < frameIndex)
            mFrameStatusSignal.wait(lock);

        if (mFrameStatus == OPENXR_FRAME_STATUS_IDLE)
        {
            Log(Debug::Verbose) << "beginFrame()";
            handleEvents();
            waitFrame();

            XrFrameBeginInfo frameBeginInfo{ XR_TYPE_FRAME_BEGIN_INFO };
            CHECK_XRCMD(xrBeginFrame(mSession, &frameBeginInfo));

            updateControls();
            updatePoses();
            mFrameStatus = OPENXR_FRAME_STATUS_READY;
        }

        assert(mFrameStatus == OPENXR_FRAME_STATUS_READY);
    }

    void OpenXRManagerImpl::viewerBarrier()
    {
        std::unique_lock<std::mutex> lock(mBarrierMutex);
        mBarrier++;

        Log(Debug::Verbose) << "mBarrier=" << mBarrier << ", tid=" << std::this_thread::get_id();

        if (mBarrier == mNBarrier)
        {
            std::unique_lock<std::mutex> lock(mFrameStatusMutex);
            mFrameStatus = OPENXR_FRAME_STATUS_ENDING;
            mFrameStatusSignal.notify_all();
            //mBarrierSignal.notify_all();
            mBarrier = 0;
        }
        //else
        //{
        //    mBarrierSignal.wait(lock, [this]() { return mBarrier == mNBarrier; });
        //}
    }

    void OpenXRManagerImpl::registerToBarrier()
    {
        std::unique_lock<std::mutex> lock(mBarrierMutex);
        mNBarrier++;
    }

    void OpenXRManagerImpl::unregisterFromBarrier()
    {
        std::unique_lock<std::mutex> lock(mBarrierMutex);
        mNBarrier--;
        assert(mNBarrier >= 0);
    }

    void
        OpenXRManagerImpl::endFrame()
    {
        if (!mSessionRunning)
            return;

        std::unique_lock<std::mutex> lock(mFrameStatusMutex);
        while(mFrameStatus != OPENXR_FRAME_STATUS_ENDING)
            mFrameStatusSignal.wait(lock);

        XrFrameEndInfo frameEndInfo{ XR_TYPE_FRAME_END_INFO };
        frameEndInfo.displayTime = mFrameState.predictedDisplayTime;
        frameEndInfo.environmentBlendMode = mEnvironmentBlendMode;
        //frameEndInfo.layerCount = (uint32_t)1;
        //frameEndInfo.layers = &mLayer_p;
        frameEndInfo.layerCount = mLayerStack.layerCount();
        frameEndInfo.layers = mLayerStack.layerHeaders();
        CHECK_XRCMD(xrEndFrame(mSession, &frameEndInfo));
        mFrameStatus = OPENXR_FRAME_STATUS_IDLE;
        mFrameIndex++;
        mFrameStatusSignal.notify_all();
    }

    std::array<XrView, 2> OpenXRManagerImpl::getStageViews()
    {
        // Get the pose of each eye in the "stage" reference space.
        // TODO: I likely won't ever use this, since it is only useful if the game world and the 
        // stage space have matching orientation, which is extremely unlikely. Instead we have
        // to keep track of the head pose separately and move our world position based on progressive 
        // changes.

        // In more detail:
        // When we apply yaw to the game world to allow free rotation of the character, the orientation 
        // of the stage space and our world space deviates which breaks free movement.
        //
        // If we align the orientations by yawing the head pose, that yaw will happen around the origin
        // of the stage space rather than the character, which will not be comfortable (or make any sense) to the player.
        //
        // If we align the orientations by yawing the view poses, the yaw will happen around the character
        // as intended, but physically walking will move the player in the wrong direction.
        //
        // The solution that solves both problems is to yaw the view pose *and* progressively track changes in head pose 
        // in the stage space and yaw that change before adding it to our separately tracked pose.

        std::array<XrView, 2> views{ {{XR_TYPE_VIEW}, {XR_TYPE_VIEW}} };
        XrViewState viewState{ XR_TYPE_VIEW_STATE };
        uint32_t viewCount = 2;

        XrViewLocateInfo viewLocateInfo{ XR_TYPE_VIEW_LOCATE_INFO };
        viewLocateInfo.viewConfigurationType = mViewConfigType;
        viewLocateInfo.displayTime = mFrameState.predictedDisplayTime;

        viewLocateInfo.space = mReferenceSpaceStage;
        CHECK_XRCMD(xrLocateViews(mSession, &viewLocateInfo, &viewState, viewCount, &viewCount, views.data()));

        return views;
    }

    std::array<XrView, 2> OpenXRManagerImpl::getHmdViews()
    {
        // Eye poses relative to the HMD rarely change. But they might 
        // if the user reconfigures his or her hmd during runtime, so we 
        // re-read this every frame anyway.
        std::array<XrView, 2> views{ {{XR_TYPE_VIEW}, {XR_TYPE_VIEW}} };
        XrViewState viewState{ XR_TYPE_VIEW_STATE };
        uint32_t viewCount = 2;

        XrViewLocateInfo viewLocateInfo{ XR_TYPE_VIEW_LOCATE_INFO };
        viewLocateInfo.viewConfigurationType = mViewConfigType;
        viewLocateInfo.displayTime = mFrameState.predictedDisplayTime;

        viewLocateInfo.space = mReferenceSpaceView;
        CHECK_XRCMD(xrLocateViews(mSession, &viewLocateInfo, &viewState, viewCount, &viewCount, views.data()));

        return views;
    }

    MWVR::Pose OpenXRManagerImpl::getLimbPose(TrackedLimb limb, TrackedSpace space)
    {
        XrSpaceLocation location{ XR_TYPE_SPACE_LOCATION };
        XrSpaceVelocity velocity{ XR_TYPE_SPACE_VELOCITY };
        location.next = &velocity;
        XrSpace limbSpace = XR_NULL_HANDLE;
        XrSpace referenceSpace = XR_NULL_HANDLE;
        switch (limb)
        {
        case TrackedLimb::HEAD:
            limbSpace = mReferenceSpaceView;
            break;
        case TrackedLimb::LEFT_HAND:
            limbSpace = mReferenceSpaceView;
            break;
        case TrackedLimb::RIGHT_HAND:
            limbSpace = mReferenceSpaceView;
            break;
        }
        switch (space)
        {
        case TrackedSpace::STAGE:
            referenceSpace = mReferenceSpaceStage;
            break;
        case TrackedSpace::VIEW:
            referenceSpace = mReferenceSpaceView;
            break;
        }
        CHECK_XRCMD(xrLocateSpace(limbSpace, referenceSpace, mFrameState.predictedDisplayTime, &location));
        if (!(velocity.velocityFlags & XR_SPACE_VELOCITY_LINEAR_VALID_BIT))
            Log(Debug::Warning) << "Unable to acquire linear velocity";
        return MWVR::Pose{
            osg::fromXR(location.pose.position),
            osg::fromXR(location.pose.orientation),
            osg::fromXR(velocity.linearVelocity)
        };
    }

    int OpenXRManagerImpl::eyes()
    {
        return mConfigViews.size();
    }

    void OpenXRManagerImpl::handleEvents()
    {
        std::unique_lock<std::mutex> lock(mEventMutex);

        // React to events
        while (auto* event = nextEvent())
        {
            Log(Debug::Verbose) << "OpenXR: Event received: " << to_string(event->type);
            switch (event->type)
            {
            case XR_TYPE_EVENT_DATA_SESSION_STATE_CHANGED:
            {
                const auto* stateChangeEvent = reinterpret_cast<const XrEventDataSessionStateChanged*>(event);
                HandleSessionStateChanged(*stateChangeEvent);
                break;
            }
            case XR_TYPE_EVENT_DATA_INSTANCE_LOSS_PENDING:
            case XR_TYPE_EVENT_DATA_INTERACTION_PROFILE_CHANGED:
            case XR_TYPE_EVENT_DATA_REFERENCE_SPACE_CHANGE_PENDING:
            default: {
                Log(Debug::Verbose) << "OpenXR: Event ignored";
                break;
            }
            }
        }
    }

    void OpenXRManagerImpl::updateControls()
    {
    }
    void
        OpenXRManagerImpl::HandleSessionStateChanged(
            const XrEventDataSessionStateChanged& stateChangedEvent)
    {
        auto oldState = mSessionState;
        auto newState = stateChangedEvent.state;
        mSessionState = newState;

        Log(Debug::Verbose) << "XrEventDataSessionStateChanged: state " << to_string(oldState) << "->" << to_string(newState);


        switch (newState)
        {
        case XR_SESSION_STATE_READY:
        case XR_SESSION_STATE_IDLE:
        {
            XrSessionBeginInfo beginInfo{ XR_TYPE_SESSION_BEGIN_INFO };
            beginInfo.primaryViewConfigurationType = mViewConfigType;
            CHECK_XRCMD(xrBeginSession(mSession, &beginInfo));
            mSessionRunning = true;
            break;
        }
        case XR_SESSION_STATE_STOPPING:
        {
            mSessionRunning = false;
            CHECK_XRCMD(xrEndSession(mSession));
            break;
        }
        default:
            Log(Debug::Verbose) << "XrEventDataSessionStateChanged: Ignoring new strate " << to_string(newState);
        }
    }

    void OpenXRManagerImpl::updatePoses()
    {
        //auto oldHeadTrackedPose = mHeadTrackedPose; 
        //auto oldLefthandPose = mLeftHandTrackedPose;
        //auto oldRightHandPose = mRightHandTrackedPose;

        //mHeadTrackedPose = getLimbPose(TrackedLimb::HEAD);
        //mLeftHandTrackedPose = getLimbPose(TrackedLimb::LEFT_HAND);
        //mRightHandTrackedPose = getLimbPose(TrackedLimb::RIGHT_HAND);

        //for (auto& cb : mPoseUpdateCallbacks)
        //{
        //    switch (cb->mLimb)
        //    {
        //    case TrackedLimb::HEAD:
        //        (*cb)(mHeadTrackedPose); break;
        //    case TrackedLimb::LEFT_HAND:
        //        (*cb)(mLeftHandTrackedPose); break;
        //    case TrackedLimb::RIGHT_HAND:
        //        (*cb)(mRightHandTrackedPose); break;
        //    }
        //}

        for (auto& cb : mPoseUpdateCallbacks)
            (*cb)(getLimbPose(cb->mLimb, cb->mSpace));
    }

    void OpenXRManagerImpl::addPoseUpdateCallback(
        osg::ref_ptr<PoseUpdateCallback> cb)
    {
        mPoseUpdateCallbacks.push_back(cb);
    }



    const XrEventDataBaseHeader*
        OpenXRManagerImpl::nextEvent()
    {
        XrEventDataBaseHeader* baseHeader = reinterpret_cast<XrEventDataBaseHeader*>(&mEventDataBuffer);
        *baseHeader = { XR_TYPE_EVENT_DATA_BUFFER };
        const XrResult result = xrPollEvent(mInstance, &mEventDataBuffer);
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

    MWVR::Pose fromXR(XrPosef pose)
    {
        return MWVR::Pose{ osg::fromXR(pose.position), osg::fromXR(pose.orientation) };
    }

    XrPosef toXR(MWVR::Pose pose)
    {
        return XrPosef{ osg::toXR(pose.orientation), osg::toXR(pose.position) };
    }
}

namespace osg
{

    Vec3 fromXR(XrVector3f v)
    {
        return Vec3{ v.x, v.y, v.z };
    }

    Quat fromXR(XrQuaternionf quat)
    {
        return Quat{ quat.x, quat.y, quat.z, quat.w };
    }

    XrVector3f toXR(Vec3 v)
    {
        return XrVector3f{ v.x(), v.y(), v.z() };
    }

    XrQuaternionf toXR(Quat quat)
    {
        return XrQuaternionf{ quat.x(), quat.y(), quat.z(), quat.w() };
    }
}
