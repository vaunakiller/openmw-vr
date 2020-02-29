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


struct XrSwapchainSubImage;


namespace MWVR
{
    struct Timer
    {
        using Measure = std::pair < const char*, int64_t >;
        using Measures = std::vector < Measure >;
        using MeasurementContext = std::pair < const char*, Measures* >;

        Timer(const char* name);
        ~Timer();
        void checkpoint(const char* name);

        std::chrono::steady_clock::time_point mBegin;
        std::chrono::steady_clock::time_point mLastCheckpoint;
        const char* mName = nullptr;
        Measures* mContext = nullptr;
    };

    //! Represents the pose of a limb in VR space.
    struct Pose
    {
        //! Position in VR space
        osg::Vec3 position{ 0,0,0 };
        //! Orientation in VR space.
        osg::Quat orientation{ 0,0,0,1 };
        //! Speed of movement in VR space, expressed in meters per second
        osg::Vec3 velocity{ 0,0,0 };
    };

    using PoseSet = std::array<Pose, 2>;

    struct PoseSets
    {
        PoseSet eye[2]{};
        PoseSet hands[2]{};
        PoseSet head{};
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
        STAGE=0, //!< Track limb in the VR stage space. Meaning a space with a floor level origin and fixed horizontal orientation.
        VIEW=1 //!< Track limb in the VR view space. Meaning a space with the head as origin and orientation.
    };

    enum class Side
    {
        LEFT_HAND = 0,
        RIGHT_HAND = 1
    };

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
        void endFrame(int64_t displayTime, class OpenXRLayerStack* layerStack);
        void updateControls();
        void playerScale(MWVR::Pose& stagePose);

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
