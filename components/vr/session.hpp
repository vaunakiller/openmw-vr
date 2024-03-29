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

#include <components/stereo/stereomanager.hpp>

#include <components/sdlutil/sdlgraphicswindow.hpp>

#include <components/settings/settings.hpp>

#include <components/vr/constants.hpp>
#include <components/vr/frame.hpp>
#include <components/vr/swapchain.hpp>
#include <components/vr/trackingpath.hpp>

#include <osg/Vec3>

namespace VR
{
    class Swapchain;
    class StageToWorldBinding;

    /// \brief Manages VR logic, such as managing frames, predicting their poses, and handling frame synchronization with the VR runtime.
    /// Should not be confused with the openxr session object.
    class Session
    {
    public:
        static Session& instance();

    public:
        Session();
        virtual ~Session();

        float playerScale() const { return mPlayerScale; }
        void computePlayerScale();

        void setCharHeight(Stereo::Unit height);
        Stereo::Unit charHeight() const { return mCharHeight; };
        Stereo::Unit playerHeight() const { return mPlayerHeight; }

        void processChangedSettings(const std::set< std::pair<std::string, std::string> >& changed);

        VR::Frame newFrame();
        void frameBeginUpdate(VR::Frame& frame);
        void frameBeginRender(VR::Frame& frame);
        void frameEnd(osg::GraphicsContext* gc, VR::Frame& frame);

        bool appShouldShareDepthInfo() const { return mAppShouldShareDepthBuffer; };

        virtual std::shared_ptr<VR::Swapchain> createSwapchain(uint32_t width, uint32_t height, uint32_t samples, uint32_t arraySize, Swapchain::Attachment attachment, const std::string& name) = 0;

        virtual std::array<Stereo::View, 2> getPredictedViews(int64_t predictedDisplayTime, VR::ReferenceSpace space) = 0;

        virtual std::array<SwapchainConfig, 2> getRecommendedSwapchainConfig() const = 0;

        void setAppShouldShareDepthBuffer(bool arg) { mAppShouldShareDepthBuffer = arg; }

        bool handDirectedMovement() const { return mHandDirectedMovement; };

        void requestRecenter(bool recenterZ);

        void instantTransition();

        VR::StageToWorldBinding& stageToWorldBinding();

        void setInteractionProfileActive(VRPath topLevelPath, bool active);
        bool getInteractionProfileActive(VRPath topLevelPath) const;

        osg::Vec3 getHandsOffset() const { return mHandsOffset; }

        Stereo::Unit getSneakOffset() const { return mSneakOffset; }

        void setSneak(bool sneak);

        void setMovementAngleOffset(osg::Vec3 offsets) { mMovementAnglesOffset = offsets; }

        const osg::Vec3& movementAngleOffset() const { return mMovementAnglesOffset; }

    protected:
        void readSettings();

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
        bool mHandDirectedMovement = false ;

        float mPlayerScale = 1.f;
        Stereo::Unit mCharHeight = Stereo::Unit::fromMeters(1.f);
        Stereo::Unit mSneakOffset = {};
        Stereo::Unit mPlayerHeight = {};

        std::set<VRPath> mActiveInteractionProfiles;

        std::unique_ptr<VR::StageToWorldBinding> mTrackerToWorldBinding;

        osg::Vec3 mHandsOffset;
        osg::Vec3 mMovementAnglesOffset;
    };

}

#endif
