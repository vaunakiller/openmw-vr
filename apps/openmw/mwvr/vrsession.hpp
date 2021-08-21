#ifndef MWVR_OPENRXSESSION_H
#define MWVR_OPENRXSESSION_H

#include <mutex>

#include <components/vr/session.hpp>

#include <openxr/openxr.h>

namespace MWVR
{

    extern void getEulerAngles(const osg::Quat& quat, float& yaw, float& pitch, float& roll);

    /// \brief Manages VR logic, such as managing frames, predicting their poses, and handling frame synchronization with the VR runtime.
    /// Should not be confused with the openxr session object.
    class OpenXRSession : public VR::Session
    {
    public:
        OpenXRSession(XrSession session, XrInstance instance, XrViewConfigurationType viewConfigType = XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO);
        ~OpenXRSession();

        void xrResourceAcquired();
        void xrResourceReleased();

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

    private:
        XrInstance mInstance;
        XrSession mSession;
        std::queue<XrEventDataBuffer> mEventQueue;
        XrViewConfigurationType mViewConfigType;
        XrSessionState mState = XR_SESSION_STATE_UNKNOWN;

        std::mutex mMutex;
        uint32_t mAcquiredResources = 0;

        bool mXrSessionShouldStop = false;
        bool mAppShouldSyncFrameLoop = false;
        bool mAppShouldRender = false;
        bool mAppShouldReadInput = false;
    };

}

#endif
