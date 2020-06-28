#ifndef MWVR_OPENRXMANAGER_H
#define MWVR_OPENRXMANAGER_H
#ifndef USE_OPENXR
#error "openxrmanager.hpp included without USE_OPENXR defined"
#endif

#include <memory>
#include <array>
#include <mutex>
#include <components/debug/debuglog.hpp>
#include <components/sdlutil/sdlgraphicswindow.hpp>
#include <components/settings/settings.hpp>
#include <osg/Camera>
#include <osgViewer/Viewer>
#include "vrtypes.hpp"


struct XrSwapchainSubImage;
struct XrCompositionLayerBaseHeader;

namespace MWVR
{
    class OpenXRManagerImpl;

    /// \brief Manage the openxr runtime and session
    class OpenXRManager : public osg::Referenced
    {
    public:
        class CleanupOperation : public osg::GraphicsOperation
        {
        public:
            CleanupOperation() : osg::GraphicsOperation("OpenXRCleanupOperation", false) {};
            void operator()(osg::GraphicsContext* gc) override;

        private:
        };


    public:
        OpenXRManager();

        ~OpenXRManager();

        /// Manager has been initialized.
        bool realized();

        //! Forward call to xrWaitFrame()
        void waitFrame();

        //! Forward call to xrBeginFrame()
        void beginFrame();

        //! Forward call to xrEndFrame()
        void endFrame(int64_t displayTime, int layerCount, const std::array<CompositionLayerProjectionView, 2>& layerStack);

        //! Whether the openxr session is currently in a running state
        bool xrSessionRunning();

        //! Process all openxr events
        void handleEvents();

        //! Instantiate implementation
        void realize(osg::GraphicsContext* gc);

        //! Enable pose predictions. Exist to police that predictions are never made out of turn.
        void enablePredictions();

        //! Disable pose predictions.
        void disablePredictions();

        //! Get poses and fov of both eyes at the predicted time, relative to the given reference space. \note Will throw if predictions are disabled.
        std::array<View, 2> getPredictedViews(int64_t predictedDisplayTime, ReferenceSpace space);

        //! Get the pose of the player's head at the predicted time, relative to the given reference space. \note Will throw if predictions are disabled.
        MWVR::Pose getPredictedHeadPose(int64_t predictedDisplayTime, ReferenceSpace space);

        //! Last predicted display time returned from xrWaitFrame();
        long long getLastPredictedDisplayTime();

        //! Last predicted display period returned from xrWaitFrame();
        long long getLastPredictedDisplayPeriod();

        //! Configuration hints for instantiating swapchains, queried from openxr.
        std::array<SwapchainConfig, 2> getRecommendedSwapchainConfig() const;

        OpenXRManagerImpl& impl() { return *mPrivate; }
        const OpenXRManagerImpl& impl() const { return *mPrivate; }

    private:
        std::shared_ptr<OpenXRManagerImpl> mPrivate;
        std::mutex mMutex;
        using lock_guard = std::lock_guard<std::mutex>;
    };
}

std::ostream& operator <<(std::ostream& os, const MWVR::Pose& pose);

#endif
