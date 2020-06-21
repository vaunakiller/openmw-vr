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
    // Use the pimpl pattern to avoid cluttering the namespace with openxr dependencies.
    class OpenXRManagerImpl;

    class OpenXRManager : public osg::Referenced
    {
    public:
        class RealizeOperation : public osg::GraphicsOperation
        {
        public:
            RealizeOperation() : osg::GraphicsOperation("OpenXRRealizeOperation", false){};
            void operator()(osg::GraphicsContext* gc) override;
            virtual bool realized();

        private:
        };

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

        bool realized();

        long long frameIndex();
        bool sessionRunning();

        void handleEvents();
        void waitFrame();
        void beginFrame();
        void endFrame(int64_t displayTime, int layerCount, const std::array<CompositionLayerProjectionView, 2>& layerStack);
        void updateControls();

        void realize(osg::GraphicsContext* gc);

        int eyes();

        void enablePredictions();
        void disablePredictions();
        std::array<View, 2> getPredictedViews(int64_t predictedDisplayTime, TrackedSpace space);
        MWVR::Pose getPredictedHeadPose(int64_t predictedDisplayTime, TrackedSpace space);
        long long getLastPredictedDisplayPeriod();

        OpenXRManagerImpl& impl() { return *mPrivate; }

    private:
        std::shared_ptr<OpenXRManagerImpl> mPrivate;
        std::mutex mMutex;
        using lock_guard = std::lock_guard<std::mutex>;
    };
}

std::ostream& operator <<(std::ostream& os, const MWVR::Pose& pose);

#endif
