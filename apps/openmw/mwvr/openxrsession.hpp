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
#include "openxrinputmanager.hpp"

namespace MWVR
{


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

        osg::ref_ptr<OpenXRManager> mXR;
        std::unique_ptr<OpenXRInputManager> mInputManager = nullptr;
        OpenXRLayerStack mLayerStack{};

        PoseSets mPredictedPoses{};

        bool mPredictionsReady{ false };
    };

}

#endif
