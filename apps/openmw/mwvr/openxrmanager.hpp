#ifndef MWVR_OPENRXMANAGER_H
#define MWVR_OPENRXMANAGER_H
#ifndef USE_OPENXR
#error "openxrmanager.hpp included without USE_OPENXR defined"
#endif

#include <memory>
#include <mutex>
#include <components/sdlutil/sdlgraphicswindow.hpp>
#include <components/settings/settings.hpp>
#include <osg/Camera>
#include <osgViewer/Viewer>


struct XrSwapchainSubImage;

namespace MWVR
{

    // Use the pimpl pattern to avoid cluttering the namespace with openxr dependencies.
    class OpenXRManagerImpl;

    class OpenXRManager : public osg::Referenced
    {
    public:
        class PoseUpdateCallback: public osg::Referenced
        {
        public:
            enum TrackedLimb
            {
                LEFT_HAND,
                RIGHT_HAND,
                HEAD
            };

            //! Describes how position is tracked. Orientation is always absolute.
            enum TrackingMode
            {
                STAGE_ABSOLUTE, //!< Position in VR stage. Meaning relative to some floor level origin
                STAGE_RELATIVE, //!< Same as STAGE_ABSOLUTE but receive only changes in position
            };

            virtual void operator()(osg::Vec3 position, osg::Quat orientation) = 0;
        };

    public:
        class RealizeOperation : public osg::GraphicsOperation
        {
        public:
            RealizeOperation(osg::ref_ptr<OpenXRManager> XR) : osg::GraphicsOperation("OpenXRRealizeOperation", false), mXR(XR) {};
            void operator()(osg::GraphicsContext* gc) override;
            bool realized();

        private:
            osg::ref_ptr<OpenXRManager> mXR;
        };

        class CleanupOperation : public osg::GraphicsOperation
        {
        public:
            CleanupOperation(osg::ref_ptr<OpenXRManager> XR) : osg::GraphicsOperation("OpenXRCleanupOperation", false), mXR(XR) {};
            void operator()(osg::GraphicsContext* gc) override;

        private:
            osg::ref_ptr<OpenXRManager> mXR;
        };

        class SwapBuffersCallback : public osg::GraphicsContext::SwapCallback
        {
        public:
            SwapBuffersCallback(osg::ref_ptr<OpenXRManager> XR) : mXR(XR) {};
            void swapBuffersImplementation(osg::GraphicsContext* gc) override;

        private:
            osg::ref_ptr<OpenXRManager> mXR;
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
        void endFrame();
        void updateControls();
        void updatePoses();

        void realize(osg::GraphicsContext* gc);

        void setPoseUpdateCallback(PoseUpdateCallback::TrackedLimb limb, PoseUpdateCallback::TrackingMode mode, osg::ref_ptr<PoseUpdateCallback> cb);

        void setViewSubImage(int eye, const ::XrSwapchainSubImage& subImage);

        int eyes();

    private:
        friend class OpenXRViewImpl;
        friend class OpenXRInputManagerImpl;
        friend class OpenXRAction;
        friend class OpenXRViewer;

        std::shared_ptr<OpenXRManagerImpl> mPrivate;
        std::mutex mMutex;
        using lock_guard = std::lock_guard<std::mutex>;
    };

}

#endif
