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
    using seconds = std::chrono::duration<double>;
    using nanoseconds = std::chrono::nanoseconds;
    using clock = std::chrono::steady_clock;
    using time_point = clock::time_point;

public:
    OpenXRSession();
    ~OpenXRSession();

    void setLayer(OpenXRLayerStack::Layer layerType, OpenXRLayer* layer);
    void swapBuffers(osg::GraphicsContext* gc);

    const PoseSets& predictedPoses() const { return mPredictedPoses; };

    //! Call before updating poses and other inputs
    void waitFrame();

    //! Update predictions
    void predictNext(int extraPeriods);

    //! Yaw angle to be used for offsetting movement direction
    float movementYaw(void);

    OpenXRLayerStack mLayerStack{};

    PoseSets mPredictedPoses{};
    bool mPredictionsReady{ false };
};

}

#endif
