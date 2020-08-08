#ifndef MWVR_OPENRXSESSION_H
#define MWVR_OPENRXSESSION_H

#include <memory>
#include <mutex>
#include <array>
#include <chrono>
#include <queue>
#include <thread>
#include <components/debug/debuglog.hpp>
#include <components/sdlutil/sdlgraphicswindow.hpp>
#include <components/settings/settings.hpp>
#include "openxrmanager.hpp"
#include "vrviewer.hpp"

namespace MWVR
{

    extern void getEulerAngles(const osg::Quat& quat, float& yaw, float& pitch, float& roll);

    /// \brief Manages VR logic, such as managing frames, predicting their poses, and handling frame synchronization with the VR runtime.
    /// Should not be confused with the openxr session object.
    class VRSession
    {
    public:
        using seconds = std::chrono::duration<double>;
        using nanoseconds = std::chrono::nanoseconds;
        using clock = std::chrono::steady_clock;
        using time_point = clock::time_point;

        //! Describes different phases of updating/rendering each frame.
        //! While two phases suffice for disambiguating current and next frame,
        //! greater granularity allows better control of synchronization
        //! with the VR runtime.
        enum class FramePhase
        {
            Update = 0, //!< The frame currently in update traversals
            Cull, //!< The frame currently in cull
            Draw, //!< The frame currently in draw
            Swap, //!< The frame being swapped
            NumPhases
        };

        struct VRFrameMeta
        {
            long long   mFrameNo{ 0 };
            long long   mPredictedDisplayTime{ 0 };
            PoseSet     mPredictedPoses{};
            bool        mShouldRender{ false };
            FrameInfo   mFrameInfo{};
        };

    public:
        VRSession();
        ~VRSession();

        void swapBuffers(osg::GraphicsContext* gc, VRViewer& viewer);

        const PoseSet& predictedPoses(FramePhase phase);

        //! Starts a new frame
        void prepareFrame();

        //! Angles to be used for overriding movement direction
        void movementAngles(float& yaw, float& pitch);

        void beginPhase(FramePhase phase);
        std::unique_ptr<VRFrameMeta>& getFrame(FramePhase phase);

        bool isRunning() const;

        float playerScale() const { return mPlayerScale; }
        float setPlayerScale(float scale) { return mPlayerScale = scale; }

        osg::Matrix viewMatrix(osg::Vec3 position, osg::Quat orientation);
        osg::Matrix viewMatrix(FramePhase phase, Side side, bool offset);
        osg::Matrix projectionMatrix(FramePhase phase, Side side);

        std::array<std::unique_ptr<VRFrameMeta>, (int)FramePhase::NumPhases> mFrame{ nullptr };

    private:
        std::mutex mMutex{};
        std::condition_variable mCondition{};
        FramePhase mXrSyncPhase{ FramePhase::Cull };

        bool mHandDirectedMovement{ false };
        bool mUseSteadyClock{ false };
        long long mFrames{ 0 };
        long long mLastRenderedFrame{ 0 };
        long long mLastPredictedDisplayTime{ 0 };
        long long mLastPredictedDisplayPeriod{ 0 };
        std::chrono::steady_clock::time_point mStart{ std::chrono::steady_clock::now() };
        std::chrono::nanoseconds mLastFrameInterval{};
        std::chrono::steady_clock::time_point mLastRenderedFrameTimestamp{ std::chrono::steady_clock::now() };

        float mPlayerScale{ 1.f };
    };

}

#endif
