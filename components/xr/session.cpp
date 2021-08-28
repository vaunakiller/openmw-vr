#include "session.hpp"
#include "instance.hpp"

#include <cassert>

namespace XR
{
    // OSG doesn't provide API to extract euler angles from a quat, but i need it.
    // Credits goes to Dennis Bunfield, i just copied his formula https://narkive.com/v0re6547.4
    void getEulerAngles(const osg::Quat& quat, float& yaw, float& pitch, float& roll)
    {
        // Now do the computation
        osg::Matrixd m2(osg::Matrixd::rotate(quat));
        double* mat = (double*)m2.ptr();
        double angle_x = 0.0;
        double angle_y = 0.0;
        double angle_z = 0.0;
        double D, C, tr_x, tr_y;
        angle_y = D = asin(mat[2]); /* Calculate Y-axis angle */
        C = cos(angle_y);
        if (fabs(C) > 0.005) /* Test for Gimball lock? */
        {
            tr_x = mat[10] / C; /* No, so get X-axis angle */
            tr_y = -mat[6] / C;
            angle_x = atan2(tr_y, tr_x);
            tr_x = mat[0] / C; /* Get Z-axis angle */
            tr_y = -mat[1] / C;
            angle_z = atan2(tr_y, tr_x);
        }
        else /* Gimball lock has occurred */
        {
            angle_x = 0; /* Set X-axis angle to zero
            */
            tr_x = mat[5]; /* And calculate Z-axis angle
            */
            tr_y = mat[4];
            angle_z = atan2(tr_y, tr_x);
        }

        yaw = angle_z;
        pitch = angle_x;
        roll = angle_y;
    }

    Session::Session(XrSession session, XrInstance instance, XrViewConfigurationType viewConfigType)
        : mInstance(instance)
        , mSession(session)
        , mViewConfigType(viewConfigType)
    {
    }

    Session::~Session()
    {
        xrDestroySession(mSession);
    }

    void Session::xrResourceAcquired()
    {
        std::scoped_lock lock(mMutex);
        mAcquiredResources++;
    }

    void Session::xrResourceReleased()
    {
        assert(mAcquiredResources != 0);

        std::scoped_lock lock(mMutex);
        mAcquiredResources--;
    }

    void Session::newFrame(uint64_t frameNo, bool& shouldSyncFrame, bool& shouldSyncInput)
    {
        handleEvents();
        shouldSyncFrame = mAppShouldSyncFrameLoop;
        shouldSyncInput = mAppShouldReadInput;
    }

    void Session::syncFrameUpdate(uint64_t frameNo, bool& shouldRender, uint64_t& predictedDisplayTime, uint64_t& predictedDisplayPeriod)
    {
        XrFrameWaitInfo frameWaitInfo{ XR_TYPE_FRAME_WAIT_INFO };
        XrFrameState frameState{ XR_TYPE_FRAME_STATE };

        CHECK_XRCMD(xrWaitFrame(mSession, &frameWaitInfo, &frameState));
        shouldRender = frameState.shouldRender && mAppShouldRender;
        predictedDisplayTime = frameState.predictedDisplayTime;
        predictedDisplayPeriod = frameState.predictedDisplayPeriod;
    }

    void Session::syncFrameRender(VR::Frame& frame)
    {
        XrFrameBeginInfo frameBeginInfo{ XR_TYPE_FRAME_BEGIN_INFO };
        CHECK_XRCMD(xrBeginFrame(mSession, &frameBeginInfo));
    }

    void Session::syncFrameEnd(VR::Frame& frame)
    {
        Instance::instance().endFrame(frame);
    }

    void Session::handleEvents()
    {
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

    const XrEventDataBaseHeader* Session::nextEvent()
    {
        if (mEventQueue.size() > 0)
            return reinterpret_cast<XrEventDataBaseHeader*> (&mEventQueue.front());
        return nullptr;
    }

    bool Session::processEvent(const XrEventDataBaseHeader* header)
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
            // TODO:
            //MWVR::Environment::get().getInputManager()->notifyInteractionProfileChanged();
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
        Session::handleSessionStateChanged(
            const XrEventDataSessionStateChanged& stateChangedEvent)
    {
        Log(Debug::Verbose) << "XrEventDataSessionStateChanged: state " << to_string(mState) << "->" << to_string(stateChangedEvent.state);
        mState = stateChangedEvent.state;

        // Ref: https://www.khronos.org/registry/OpenXR/specs/1.0/html/xrspec.html#session-states
        switch (mState)
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
            Log(Debug::Warning) << "XrEventDataSessionStateChanged: Ignoring new state " << to_string(mState);
        }

        return true;
    }

    bool Session::checkStopCondition()
    {
        return mAcquiredResources == 0;
    }

    bool Session::xrNextEvent(XrEventDataBuffer& eventBuffer)
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

    void Session::popEvent()
    {
        if (mEventQueue.size() > 0)
            mEventQueue.pop();
    }

    void
        Session::xrQueueEvents()
    {
        XrEventDataBuffer eventBuffer;
        while (xrNextEvent(eventBuffer))
        {
            mEventQueue.push(eventBuffer);
        }
    }
}

