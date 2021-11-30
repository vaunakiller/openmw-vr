#ifndef XR_SESSION_HPP
#define XR_SESSION_HPP

#include <mutex>

#include <components/vr/session.hpp>
#include <components/vr/constants.hpp>

#include <openxr/openxr.h>

namespace VR
{
    class StageToWorldBinding;
}

namespace XR
{
    class Tracker;

    extern void getEulerAngles(const osg::Quat& quat, float& yaw, float& pitch, float& roll);

    /// \brief Manages VR logic, such as managing frames, predicting their poses, and handling frame synchronization with the VR runtime.
    /// Should not be confused with the openxr session object.
    class Session : public VR::Session
    {
    public:
        static Session& instance();

    public:
        Session(XrSession session, XrViewConfigurationType viewConfigType = XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO);
        ~Session();

        void xrResourceAcquired();
        void xrResourceReleased();

        XrSession xrSession() { return mXrSession; };

        XrSpace getReferenceSpace(VR::ReferenceSpace space);

        XR::Tracker& tracker() { return *mTracker; }
        VR::StageToWorldBinding& stageToWorldBinding() { return *mTrackerToWorldBinding; }
        std::array<Stereo::View, 2> getPredictedViews(int64_t predictedDisplayTime, VR::ReferenceSpace space) override;

        void xrDebugSetNames();

        std::array<VR::SwapchainConfig, 2> getRecommendedSwapchainConfig() const override;

        bool runtimeSupportsFormat(int64_t format) const override;

    protected:
        void newFrame(uint64_t frameNo, bool& shouldSyncFrame, bool& shouldSyncInput) override;
        void syncFrameUpdate(uint64_t frameNo, bool& shouldRender, uint64_t& predictedDisplayTime, uint64_t& predictedDisplayPeriod) override;
        void syncFrameRender(VR::Frame& frame) override;
        void syncFrameEnd(VR::Frame& frame) override;

        void handleEvents();
        bool xrNextEvent(XrEventDataBuffer& eventBuffer);
        void xrQueueEvents();
        const XrEventDataBaseHeader* nextEvent();
        bool processEvent(const XrEventDataBaseHeader* header);
        void popEvent();
        bool handleSessionStateChanged(const XrEventDataSessionStateChanged& stateChangedEvent);
        bool checkStopCondition();

        void init();
        void cleanup();

        void createXrReferenceSpaces();
        void destroyXrReferenceSpaces();
        void logXrReferenceSpaces();

        void createXrTracker();
        void initCompositionLayerDepth();
        void initMSFTReprojection();

        void destroyXrSession();

        VR::Swapchain* createSwapchain(uint32_t width, uint32_t height, uint32_t samples, uint32_t arraySize, VR::SwapchainUse use, const std::string& name, int64_t preferredFormat) override;

    private:
        XrSession mXrSession;
        std::queue<XrEventDataBuffer> mEventQueue;
        XrViewConfigurationType mViewConfigType;
        XrSessionState mState = XR_SESSION_STATE_UNKNOWN;

        XrSpace mReferenceSpaceView = XR_NULL_HANDLE;
        XrSpace mReferenceSpaceStage = XR_NULL_HANDLE;
        XrSpace mReferenceSpaceLocal = XR_NULL_HANDLE;

        std::unique_ptr<XR::Tracker> mTracker;
        std::unique_ptr<VR::StageToWorldBinding> mTrackerToWorldBinding{ nullptr };

        std::mutex mMutex;
        uint32_t mAcquiredResources = 0;

        bool mXrSessionShouldStop = false;
        bool mAppShouldSyncFrameLoop = false;
        bool mAppShouldRender = false;
        bool mAppShouldReadInput = false;

        bool mMSFTReprojectionModeDepth = false;
    };

}

#endif
