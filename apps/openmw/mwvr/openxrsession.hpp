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

        PoseSets& predictedPoses() { return mPredictedPoses; };

        //! Call before updating poses
        void waitFrame();

        //! Update predictions
        void predictNext(int extraPeriods);

        OpenXRLayerStack mLayerStack{};
        osg::ref_ptr<OpenXRManager> mXR;

        PoseSets mPredictedPoses{};

        bool mPredictionsReady;
    };

}

#endif
