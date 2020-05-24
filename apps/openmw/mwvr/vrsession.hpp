#ifndef MWVR_OPENRXSESSION_H
#define MWVR_OPENRXSESSION_H

#include <memory>
#include <mutex>
#include <array>
#include <chrono>
#include <queue>
#include <components/debug/debuglog.hpp>
#include <components/sdlutil/sdlgraphicswindow.hpp>
#include <components/settings/settings.hpp>
#include "openxrmanager.hpp"
#include "vrviewer.hpp"

namespace MWVR
{

extern void getEulerAngles(const osg::Quat& quat, float& yaw, float& pitch, float& roll);

class VRSession
{
public:
    using seconds = std::chrono::duration<double>;
    using nanoseconds = std::chrono::nanoseconds;
    using clock = std::chrono::steady_clock;
    using time_point = clock::time_point;

    enum class FramePhase
    {
        Predraw = 0, //!< Get poses predicted for the next frame to be drawn
        Draw = 1, //!< Get poses predicted for the current rendering frame
        Postdraw = 2, //!< Get poses predicted for the last rendered frame
    };

    struct VRFrame
    {
        long long   mFrameNo{ 0 };
        long long   mPredictedDisplayTime{ 0 };
        PoseSet     mPredictedPoses{};
    };

public:
    VRSession();
    ~VRSession();

    void swapBuffers(osg::GraphicsContext* gc, VRViewer& viewer);

    const PoseSet& predictedPoses(FramePhase phase);

    //! Starts a new frame
    void startFrame();

    //! Angles to be used for overriding movement direction
    void movementAngles(float& yaw, float& pitch);

    void advanceFramePhase(void);

    bool isRunning() const;

    float playerScale() const { return mPlayerScale; }
    float setPlayerScale(float scale) { return mPlayerScale = scale; }

    osg::Matrix viewMatrix(FramePhase phase, Side side);
    osg::Matrix projectionMatrix(FramePhase phase, Side side);

    std::unique_ptr<VRFrame> mPredrawFrame{ nullptr };
    std::unique_ptr<VRFrame> mDrawFrame{ nullptr };
    std::unique_ptr<VRFrame> mPostdrawFrame{ nullptr };

    std::mutex mMutex{};
    std::condition_variable mCondition{};

    long long mFrames{ 0 };
    long long mLastRenderedFrame{ 0 };

    float mPlayerScale{ 1.f };
};

}

#endif
