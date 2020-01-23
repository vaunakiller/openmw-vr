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
    //! Represents the pose of a limb in VR space.
    struct Pose
    {
        //! Position in VR space
        osg::Vec3 position;
        //! Orientation in VR space.
        osg::Quat orientation;
        //! Speed of movement in VR space, expressed in meters per second
        osg::Vec3 velocity;
    };

    //! Describes what limb to track.
    enum class TrackedLimb
    {
        LEFT_HAND,
        RIGHT_HAND,
        HEAD
    };

    //! Describes what space to track the limb in
    enum class TrackedSpace
    {
        STAGE, //!< Track limb in the VR stage space. Meaning a space with a floor level origin and fixed horizontal orientation.
        VIEW //!< Track limb in the VR view space. Meaning a space with the head as origin and orientation.
    };


    // Use the pimpl pattern to avoid cluttering the namespace with openxr dependencies.
    class OpenXRManagerImpl;

    class OpenXRManager : public osg::Referenced
    {
    public:
        class PoseUpdateCallback: public osg::Referenced
        {
        public:
            PoseUpdateCallback(TrackedLimb limb, TrackedSpace space)
                : mLimb(limb), mSpace(space){}

            virtual void operator()(MWVR::Pose pose) = 0;

            TrackedLimb mLimb;
            TrackedSpace mSpace;
        };

    public:
        class RealizeOperation : public osg::GraphicsOperation
        {
        public:
            RealizeOperation(osg::ref_ptr<OpenXRManager> XR) : osg::GraphicsOperation("OpenXRRealizeOperation", false), mXR(XR) {};
            void operator()(osg::GraphicsContext* gc) override;
            virtual bool realized();

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

        void addPoseUpdateCallback(osg::ref_ptr<PoseUpdateCallback> cb);

        int eyes();

        //! A barrier used internally to ensure all views have released their frames before endFrame can complete.
        void viewerBarrier();
        //! Increments the target viewer counter of the barrier
        void registerToBarrier();
        //! Decrements the target viewer counter of the barrier
        void unregisterFromBarrier();

        OpenXRManagerImpl& impl() { return *mPrivate; }

    private:
        std::shared_ptr<OpenXRManagerImpl> mPrivate;
        std::mutex mMutex;
        using lock_guard = std::lock_guard<std::mutex>;
    };

#ifndef _NDEBUG
    class PoseLogger : public OpenXRManager::PoseUpdateCallback
    {
    public:
        PoseLogger(TrackedLimb limb, TrackedSpace space) 
            : OpenXRManager::PoseUpdateCallback(limb, space) {};
        void operator()(MWVR::Pose pose) override;
    };
#endif
}

std::ostream& operator <<(std::ostream& os, const MWVR::Pose& pose);

#endif
