#ifndef MWVR_OPENRXSESSION_H
#define MWVR_OPENRXSESSION_H

#include <memory>
#include <mutex>
#include <array>
#include <chrono>
#include <queue>
#include <thread>
#include <condition_variable>
#include <components/debug/debuglog.hpp>
#include <components/sdlutil/sdlgraphicswindow.hpp>
#include <components/settings/settings.hpp>
#include <components/vr/frame.hpp>
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

    public:
        VRSession();
        ~VRSession();

        bool seatedPlay() const { return mSeatedPlay; }

        float playerScale() const { return mPlayerScale; }
        void setPlayerScale(float scale) { mPlayerScale = scale; }

        float eyeLevel() const { return mEyeLevel; }
        void setEyeLevel(float eyeLevel) { mEyeLevel = eyeLevel; }

        void processChangedSettings(const std::set< std::pair<std::string, std::string> >& changed);

        VR::Frame newFrame();
        void frameBeginUpdate(VR::Frame& frame);
        void frameBeginRender(VR::Frame& frame);
        void frameEnd(osg::GraphicsContext* gc, VRViewer& viewer, VR::Frame& frame);

    protected:
        void setSeatedPlay(bool seatedPlay);

    private:
        bool mSeatedPlay{ false };
        long long mFrames{ 0 };
        long long mLastRenderedFrame{ 0 };
        long long mLastPredictedDisplayTime{ 0 };
        long long mLastPredictedDisplayPeriod{ 0 };
        std::chrono::steady_clock::time_point mStart{ std::chrono::steady_clock::now() };
        std::chrono::nanoseconds mLastFrameInterval{};
        std::chrono::steady_clock::time_point mLastRenderedFrameTimestamp{ std::chrono::steady_clock::now() };

        float mPlayerScale{ 1.f };
        float mEyeLevel{ 1.f };
    };

}

#endif
