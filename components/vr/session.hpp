#ifndef VR_SESSION_HPP
#define VR_SESSION_HPP

#include <memory>
#include <mutex>
#include <array>
#include <chrono>
#include <queue>
#include <thread>
#include <condition_variable>

#include <components/debug/debuglog.hpp>

#include <components/misc/stereo.hpp>

#include <components/sdlutil/sdlgraphicswindow.hpp>

#include <components/settings/settings.hpp>

#include <components/vr/constants.hpp>
#include <components/vr/frame.hpp>
#include <components/vr/swapchain.hpp>

namespace VR
{
    class Swapchain;

    /// \brief Manages VR logic, such as managing frames, predicting their poses, and handling frame synchronization with the VR runtime.
    /// Should not be confused with the openxr session object.
    class Session
    {
    public:
        static Session& instance();

    public:
        Session();
        virtual ~Session();

        bool seatedPlay() const { return mSeatedPlay; }

        float playerScale() const { return mPlayerScale; }
        void setPlayerScale(float scale) { mPlayerScale = scale; }

        float eyeLevel() const { return mEyeLevel; }
        void setEyeLevel(float eyeLevel) { mEyeLevel = eyeLevel; }

        void processChangedSettings(const std::set< std::pair<std::string, std::string> >& changed);

        VR::Frame newFrame();
        void frameBeginUpdate(VR::Frame& frame);
        void frameBeginRender(VR::Frame& frame);
        void frameEnd(osg::GraphicsContext* gc, VR::Frame& frame);

        bool appShouldShareDepthInfo() const { return mAppShouldShareDepthBuffer; };

        virtual VR::Swapchain* createSwapchain(uint32_t width, uint32_t height, uint32_t samples, uint32_t arraySize, SwapchainUse use, const std::string& name, int64_t preferredFormat = 0) = 0;

        virtual std::array<Misc::View, 2> getPredictedViews(int64_t predictedDisplayTime, VR::ReferenceSpace space) = 0;

        virtual std::array<SwapchainConfig, 2> getRecommendedSwapchainConfig() const = 0;

        virtual bool runtimeSupportsFormat(int64_t format) const = 0;

        void setAppShouldShareDepthBuffer(bool arg) { mAppShouldShareDepthBuffer = arg; }

    protected:
        void setSeatedPlay(bool seatedPlay);

        //! Called once when initializing a new frame. The implementation *must* set shouldSyncFrame and shouldSyncInput. If shouldSyncFrame is set to false by the implementation, syncFrame* will be called for this frame.
        virtual void newFrame(uint64_t frameNo, bool& shouldSyncFrame, bool& shouldSyncInput) = 0;

        //! Called once immediately after newFrame(uint64_t, bool) if shouldSyncFrame was true.
        //! The implementation *must* set shouldRender, predictedDisplayTime, and predictedDisplayPeriod.
        //! This is where OpenXR implementations must call xrWaitFrame()
        virtual void syncFrameUpdate(uint64_t frameNo, bool& shouldRender, uint64_t& predictedDisplayTime, uint64_t& predictedDisplayPeriod) = 0;

        //! Called once immediately before render work begins.
        //! This is where OpenXR implementations must call xrBeginFrame()
        virtual void syncFrameRender(VR::Frame& frame) = 0;

        //! Called once immediately after render work is complete.
        //! This is where OpenXR implementations must call xrEndFrame()
        virtual void syncFrameEnd(VR::Frame& frame) = 0;

    private:

        bool mAppShouldShareDepthBuffer = false;

        bool mSeatedPlay{ false };
        float mPlayerScale{ 1.f };
        float mEyeLevel{ 1.f };
    };

}

#endif
