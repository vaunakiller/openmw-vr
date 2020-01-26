#ifndef MWVR_OPENRXMANAGER_H
#define MWVR_OPENRXMANAGER_H
#ifndef USE_OPENXR
#error "openxrmanager.hpp included without USE_OPENXR defined"
#endif

#include <memory>
#include <mutex>
#include <components/debug/debuglog.hpp>
#include <components/sdlutil/sdlgraphicswindow.hpp>
#include <components/settings/settings.hpp>
#include <osg/Camera>
#include <osgViewer/Viewer>


struct XrSwapchainSubImage;


namespace MWVR
{
    struct Timer
    {
        Timer(std::string name) : mName(name) 
        {
            mBegin = std::chrono::steady_clock::now();
        }
        ~Timer()
        {
            std::chrono::steady_clock::time_point end = std::chrono::steady_clock::now();
            auto elapsed = std::chrono::duration_cast<std::chrono::duration<double>>(end - mBegin);
            Log(Debug::Verbose) << mName << "Elapsed: " << elapsed.count() << "s";
        }

        std::chrono::steady_clock::time_point mBegin;
        std::string mName;
    };

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

    struct OpenXRFrameIndexer
    {
        static OpenXRFrameIndexer& instance();

        OpenXRFrameIndexer() = default;
        ~OpenXRFrameIndexer() = default;

        int64_t advanceUpdateIndex();

        int64_t renderIndex() { return mRenderIndex; }

        int64_t advanceRenderIndex();

        int64_t updateIndex() { return mUpdateIndex; }

        std::mutex mMutex{};
        int64_t mUpdateIndex{ -1 };
        int64_t mRenderIndex{ -1 };
    };


    // Use the pimpl pattern to avoid cluttering the namespace with openxr dependencies.
    class OpenXRManagerImpl;

    class OpenXRManager : public osg::Referenced
    {
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

        void realize(osg::GraphicsContext* gc);

        int eyes();

        OpenXRManagerImpl& impl() { return *mPrivate; }

    private:
        std::shared_ptr<OpenXRManagerImpl> mPrivate;
        std::mutex mMutex;
        using lock_guard = std::lock_guard<std::mutex>;
    };
}

std::ostream& operator <<(std::ostream& os, const MWVR::Pose& pose);

#endif
