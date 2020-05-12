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

extern void getEulerAngles(const osg::Quat& quat, float& yaw, float& pitch, float& roll);

class OpenXRSession
{
public:
    using seconds = std::chrono::duration<double>;
    using nanoseconds = std::chrono::nanoseconds;
    using clock = std::chrono::steady_clock;
    using time_point = clock::time_point;

    enum class PredictionSlice
    {
        Predraw = 0, //!< Get poses predicted for the next frame to be drawn
        Draw = 1, //!< Get poses predicted for the current rendering
        NumSlices
    };

public:
    OpenXRSession();
    ~OpenXRSession();

    void setLayer(OpenXRLayerStack::Layer layerType, OpenXRLayer* layer);
    void swapBuffers(osg::GraphicsContext* gc);

    const PoseSets& predictedPoses(PredictionSlice slice);

    //! Call before updating poses and other inputs
    void waitFrame();

    //! Update predictions
    void predictNext(int extraPeriods);

    //! Angles to be used for overriding movement direction
    void movementAngles(float& yaw, float& pitch);

    void advanceFrame(void);

    bool isRunning() { return mIsRunning; }
    bool shouldRender() { return mShouldRender; }

    OpenXRLayerStack mLayerStack{};

    PoseSets mPredictedPoses[(int)PredictionSlice::NumSlices]{};

    int mRenderFrame{ 0 };
    int mRenderedFrames{ 0 };
    int mPredictionFrame{ 1 };
    int mPredictedFrames{ 0 };

    bool mIsRunning{ false };
    bool mShouldRender{ false };
};

}

#endif
