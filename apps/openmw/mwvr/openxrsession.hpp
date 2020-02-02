#ifndef MWVR_OPENRXSESSION_H
#define MWVR_OPENRXSESSION_H

#include <memory>
#include <mutex>
#include <array>
#include <chrono>
#include <deque>
#include <components/debug/debuglog.hpp>
#include <components/sdlutil/sdlgraphicswindow.hpp>
#include <components/settings/settings.hpp>
#include "openxrmanager.hpp"
#include "openxrlayer.hpp"

namespace MWVR
{

    using PoseSet = std::array<Pose, 2>;

    struct PoseSets
    {
        PoseSet eye[2]{};
        PoseSet hands[2]{};
        PoseSet head{};
    };

    class OpenXRSession
    {
        using seconds = std::chrono::duration<double>;
        using nanoseconds = std::chrono::nanoseconds;
        using clock = std::chrono::steady_clock;
        using time_point = clock::time_point;

    public:
        OpenXRSession(osg::ref_ptr<OpenXRManager> XR);
        ~OpenXRSession();

        void setLayer(OpenXRLayerStack::Layer layerType, OpenXRLayer* layer);
        void swapBuffers(osg::GraphicsContext* gc);
        void run();

        PoseSets& predictedPoses() { return mPredictedPoses; };

        //! Call before rendering to update predictions
        void waitFrame();

        //! Most recent prediction for display time of next frame.
        int64_t predictedDisplayTime() { return mPredictedDisplayTime; };

        //! Update predictions
        void predictNext(int extraPeriods);

        OpenXRLayerStack mLayerStack{};
        osg::ref_ptr<OpenXRManager> mXR;
        std::thread mXRThread;
        bool mShouldQuit = false;

        PoseSets mPredictedPoses{};

        int64_t mPredictedDisplayTime{ 0 };
        time_point mPredictedDisplayTimeRealTime{ nanoseconds(0) };
        std::deque<int64_t> mRenderTimes;
        int64_t mPredictedPeriods{ 0 };
        double mFps{ 0. };
        time_point mFrameEndTimePoint;
        time_point mFrameBeginTimePoint;

        bool mNeedSync = true;
        std::condition_variable mSync{};
        std::mutex mSyncMutex{};

        bool mShouldRenderLayers = false;
        std::condition_variable mRenderSignal{};
        std::mutex mRenderMutex{};
    };

}

#endif
