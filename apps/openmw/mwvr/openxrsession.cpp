#include "openxrmanager.hpp"
#include "openxrmanagerimpl.hpp"
#include "openxrswapchain.hpp"
#include "../mwinput/inputmanagerimp.hpp"

#include <components/debug/debuglog.hpp>
#include <components/sdlutil/sdlgraphicswindow.hpp>

#include <Windows.h>

#include <openxr/openxr.h>
#include <openxr/openxr_platform.h>
#include <openxr/openxr_platform_defines.h>
#include <openxr/openxr_reflection.h>

#include <osg/Camera>

#include <vector>
#include <array>
#include <iostream>
#include "openxrsession.hpp"
#include <time.h>

namespace MWVR
{
    OpenXRSession::OpenXRSession(
        osg::ref_ptr<OpenXRManager> XR)
        : mXR(XR)
        , mFrameEndTimePoint(clock::now())
        , mFrameBeginTimePoint(mFrameEndTimePoint)
    {
        mXRThread = std::thread([this] {run(); });
    }

    OpenXRSession::~OpenXRSession()
    {
        mShouldQuit = true;
    }

    void OpenXRSession::setLayer(
        OpenXRLayerStack::Layer layerType, 
        OpenXRLayer* layer)
    {
        mLayerStack.setLayer(layerType, layer);
    }

    void OpenXRSession::swapBuffers(osg::GraphicsContext* gc)
    {
        Log(Debug::Verbose) << "OpenXRSession::swapBuffers";
        Timer timer("OpenXRSession::SwapBuffers");
        static int wat = 0;
        if (!mXR->sessionRunning())
            return;
        if (!mShouldRenderLayers)
        {
            return;
        }

        //if (wat++ % 100 != 0)
        //    return;


        //std::unique_lock<std::mutex> renderLock(mRenderMutex);
        //timer.checkpoint("Mutex");
        for (auto layer : mLayerStack.layerObjects())
            layer->swapBuffers(gc);
        mFrameEndTimePoint = clock::now();
        auto nanoseconds_elapsed = std::chrono::duration_cast<std::chrono::nanoseconds>(mFrameEndTimePoint - mFrameBeginTimePoint);
        auto milliseconds_elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(nanoseconds_elapsed);
        mRenderTimes.push_back(nanoseconds_elapsed.count());
        Log(Debug::Verbose) << "Render time: " << milliseconds_elapsed.count() << "ms";
        //mShouldRenderLayers = true;
        timer.checkpoint("Rendered");

        mXR->endFrame(predictedDisplayTime(), &mLayerStack);
        Log(Debug::Verbose) << "mFrameEndTimePoint:            " << mFrameEndTimePoint.time_since_epoch().count() / 1000000;
        Log(Debug::Verbose) << "mFrameBeginTimePoint:          " << mFrameBeginTimePoint.time_since_epoch().count() / 1000000;
        Log(Debug::Verbose) << "mPredictedDisplayTimeRealTime: " << mPredictedDisplayTimeRealTime.time_since_epoch().count() / 1000000;
        Log(Debug::Verbose) << "mPredictedTime:                " << predictedDisplayTime() / 1000000;
        Log(Debug::Verbose) << "mXrDisplayTime:                " << mXR->impl().frameState().predictedDisplayTime / 1000000;
        //mShouldRenderLayers = false;
        mFrameBeginTimePoint = clock::now();

        //mRenderTimes.push_front(std::chrono::duration_cast<std::chrono::nanoseconds>(mFrameEndTimePoint - mFrameBeginTimePoint).count());
        //if(mRenderTimes.size() > 33) mRenderTimes.pop_back();

        //if (mPredictedPeriod == 0)
        //{
        //    mPredictedPeriod = milliseconds_elapsed;
        //    mPredictedPeriods = 1;
        //}
        //else
        //{
        //    double periodDevianceMagnitude = static_cast<double>(std::max(milliseconds_elapsed, mPredictedPeriod)) / static_cast<double>(std::min(milliseconds_elapsed, mPredictedPeriod));
        //    double periodStability = std::pow(periodDevianceMagnitude, -2.);
        //    mPredictedPeriod = (1. - periodStability) * mPredictedPeriod + (periodStability)*milliseconds_elapsed;
        //}

    }

    void OpenXRSession::run()
    {
    }

    void OpenXRSession::waitFrame()
    {
        // For now it seems we must just accept crap performance from the rendering loop
        // Since Oculus' implementation of waitFrame() does not even attempt to reflect real
        // render time and just incurs a huge useless delay.

        Log(Debug::Verbose) << "OpenXRSesssion::beginframe";
        Timer timer("OpenXRSession::beginFrame");
        mXR->waitFrame();
        timer.checkpoint("waitFrame");
        predictNext(0);
        mShouldRenderLayers = true;

    }

    void OpenXRSession::predictNext(int extraPeriods)
    {
        int64_t sum = 0;
        while (mRenderTimes.size() > 10)
            mRenderTimes.pop_front();
        for (unsigned i = 0; i < mRenderTimes.size(); i++)
        {
            sum += mRenderTimes[i] / mXR->impl().frameState().predictedDisplayPeriod;
        }
        int64_t average = sum / std::max((int64_t)mRenderTimes.size(), (int64_t)1);

        mPredictedPeriods = std::max(average, (int64_t)0) + extraPeriods;

        Log(Debug::Verbose) << "Periods: " << mPredictedPeriods;

        int64_t future = (mPredictedPeriods)*mXR->impl().frameState().predictedDisplayPeriod;

        mPredictedDisplayTime = mXR->impl().frameState().predictedDisplayTime + future;
        mPredictedDisplayTimeRealTime = mFrameBeginTimePoint + nanoseconds(future + mXR->impl().frameState().predictedDisplayPeriod);

        // Update pose predictions
        mPredictedPoses.head[(int)TrackedSpace::STAGE] = mXR->impl().getPredictedLimbPose(mPredictedDisplayTime, TrackedLimb::HEAD, TrackedSpace::STAGE);
        mPredictedPoses.head[(int)TrackedSpace::VIEW] = mXR->impl().getPredictedLimbPose(mPredictedDisplayTime, TrackedLimb::HEAD, TrackedSpace::VIEW);
        mPredictedPoses.hands[(int)Chirality::LEFT_HAND][(int)TrackedSpace::STAGE] = mXR->impl().getPredictedLimbPose(mPredictedDisplayTime, TrackedLimb::LEFT_HAND, TrackedSpace::STAGE);
        mPredictedPoses.hands[(int)Chirality::LEFT_HAND][(int)TrackedSpace::VIEW] = mXR->impl().getPredictedLimbPose(mPredictedDisplayTime, TrackedLimb::LEFT_HAND, TrackedSpace::VIEW);
        mPredictedPoses.hands[(int)Chirality::RIGHT_HAND][(int)TrackedSpace::STAGE] = mXR->impl().getPredictedLimbPose(mPredictedDisplayTime, TrackedLimb::RIGHT_HAND, TrackedSpace::STAGE);
        mPredictedPoses.hands[(int)Chirality::RIGHT_HAND][(int)TrackedSpace::VIEW] = mXR->impl().getPredictedLimbPose(mPredictedDisplayTime, TrackedLimb::RIGHT_HAND, TrackedSpace::VIEW);
        auto stageViews = mXR->impl().getPredictedViews(mPredictedDisplayTime, TrackedSpace::STAGE);
        auto hmdViews = mXR->impl().getPredictedViews(mPredictedDisplayTime, TrackedSpace::VIEW);
        mPredictedPoses.eye[(int)Chirality::LEFT_HAND][(int)TrackedSpace::STAGE] = fromXR(stageViews[(int)Chirality::LEFT_HAND].pose);
        mPredictedPoses.eye[(int)Chirality::LEFT_HAND][(int)TrackedSpace::VIEW] = fromXR(hmdViews[(int)Chirality::LEFT_HAND].pose);
        mPredictedPoses.eye[(int)Chirality::RIGHT_HAND][(int)TrackedSpace::STAGE] = fromXR(stageViews[(int)Chirality::RIGHT_HAND].pose);
        mPredictedPoses.eye[(int)Chirality::RIGHT_HAND][(int)TrackedSpace::VIEW] = fromXR(hmdViews[(int)Chirality::RIGHT_HAND].pose);

        //std::unique_lock<std::mutex> lock(mSyncMutex);
        //mSync.notify_all();
    }
}

std::ostream& operator <<(
    std::ostream& os,
    const MWVR::Pose& pose)
{
    os << "position=" << pose.position << " orientation=" << pose.orientation << " velocity=" << pose.velocity;
    return os;
}

