#include "vrenvironment.hpp"
#include "openxrmanager.hpp"
#include "openxrmanagerimpl.hpp"
#include "../mwinput/inputmanagerimp.hpp"

#include <components/debug/debuglog.hpp>
#include <components/sdlutil/sdlgraphicswindow.hpp>

#include <Windows.h>

#include <openxr/openxr.h>
#include <openxr/openxr_platform.h>
#include <openxr/openxr_platform_defines.h>
#include <openxr/openxr_reflection.h>

#include <osg/Camera>

#include <vector>
#include <array>
#include <map>
#include <iostream>

namespace MWVR
{
    

    static std::vector<Timer::MeasurementContext> stats;
    static std::mutex statsMutex;
    static std::thread statsThread;
    static bool statsThreadRunning = false;

    static void statsThreadRun()
    {
        while (statsThreadRunning)
        {
            std::stringstream ss;
            for (auto& context : stats)
            {
                for (auto& measurement : *context.second)
                {
                    double ms = static_cast<double>(measurement.second) / 1000000.;
                    Log(Debug::Verbose) << context.first << "." << measurement.first << ": " << ms << "ms";
                }
            }

            //Log(Debug::Verbose) << ss.str();

            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
    }



    Timer::Timer(const char* name) : mName(name)
    {
        mLastCheckpoint = mBegin = std::chrono::steady_clock::now();

        std::unique_lock<std::mutex> lock(statsMutex);
        for (auto& m : stats)
        {
            if (m.first == mName)
                mContext = m.second;
        }

        if (mContext == nullptr)
        {
            mContext = new Measures();
            mContext->reserve(32);
            stats.emplace_back(MeasurementContext(mName, mContext));
        }

        if (!statsThreadRunning)
        {
            statsThreadRunning = true;
            statsThread = std::thread([] { statsThreadRun(); });
        }
    }
    Timer::~Timer()
    {
        //statsThreadRunning = false;
        checkpoint("~");
    }

    void Timer::checkpoint(const char* name)
    {
        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::nanoseconds>(now - mLastCheckpoint);
        mLastCheckpoint = now;

        Measure* measure = nullptr;
        for (auto& m : *mContext)
        {
            if (m.first == name)
            {
                measure = &m;
            }
        }
        if (!measure)
        {
            mContext->push_back(Measure(name, elapsed.count()));
        }
        else {
            measure->second = measure->second * 0.95 + elapsed.count() * 0.05;
        }
    }

    OpenXRManager::OpenXRManager()
        : mPrivate(nullptr)
        , mMutex()
    {

    }

    OpenXRManager::~OpenXRManager()
    {

    }

    bool
        OpenXRManager::realized()
    {
        return !!mPrivate;
    }

    long long
        OpenXRManager::frameIndex()
    {
        if (realized())
            return impl().mFrameIndex;
        return -1;
    }

    bool OpenXRManager::sessionRunning()
    {
        if (realized())
            return impl().mSessionRunning;
        return false;
    }

    void OpenXRManager::handleEvents()
    {
        if (realized())
            return impl().handleEvents();
    }

    void OpenXRManager::waitFrame()
    {
        if (realized())
            return impl().waitFrame();
    }

    void OpenXRManager::beginFrame()
    {
        if (realized())
            return impl().beginFrame();
    }

    void OpenXRManager::endFrame(int64_t displayTime, int layerCount, XrCompositionLayerBaseHeader** layerStack)
    {
        if (realized())
            return impl().endFrame(displayTime, layerCount, layerStack);
    }

    void OpenXRManager::updateControls()
    {
        if (realized())
            return impl().updateControls();
    }

    void
        OpenXRManager::realize(
            osg::GraphicsContext* gc)
    {
        lock_guard lock(mMutex);
        if (!realized())
        {
            gc->makeCurrent();
            try {
                mPrivate = std::make_shared<OpenXRManagerImpl>();
            }
            catch (std::exception & e)
            {
                Log(Debug::Error) << "Exception thrown by OpenXR: " << e.what();
                osg::ref_ptr<osg::State> state = gc->getState();

            }
        }
    }

    int OpenXRManager::eyes()
    {
        if (realized())
            return impl().eyes();
        return 0;
    }

    void
        OpenXRManager::RealizeOperation::operator()(
            osg::GraphicsContext* gc)
    {
        auto* xr = Environment::get().getManager();
        xr->realize(gc);
    }

    bool
        OpenXRManager::RealizeOperation::realized()
    {
        auto* xr = Environment::get().getManager();
        return xr->realized();
    }

    void
        OpenXRManager::CleanupOperation::operator()(
            osg::GraphicsContext* gc)
    {

    }

    Pose Pose::operator+(const Pose& rhs)
    {
        Pose pose = *this;
        pose.position += this->orientation * rhs.position;
        pose.orientation = rhs.orientation * this->orientation;
        return pose;
    }

    const Pose& Pose::operator+=(const Pose& rhs)
    {
        *this = *this + rhs;
        return *this;
    }

    Pose Pose::operator*(float scalar)
    {
        Pose pose = *this;
        pose.position *= scalar;
        pose.velocity *= scalar;
        return pose;
    }

    const Pose& Pose::operator*=(float scalar)
    {
        *this = *this * scalar;
        return *this;
    }

    // near and far named with an underscore because of galaxy brain defines in included windows headers.
    osg::Matrix FieldOfView::perspectiveMatrix(float near_, float far_)
    {
        const float tanLeft = tanf(angleLeft);
        const float tanRight = tanf(angleRight);
        const float tanDown = tanf(angleDown);
        const float tanUp = tanf(angleUp);

        const float tanWidth = tanRight - tanLeft;
        const float tanHeight = tanUp - tanDown;

        const float offset = near_;

        float matrix[16] = {};

        matrix[0] = 2 / tanWidth;
        matrix[4] = 0;
        matrix[8] = (tanRight + tanLeft) / tanWidth;
        matrix[12] = 0;

        matrix[1] = 0;
        matrix[5] = 2 / tanHeight;
        matrix[9] = (tanUp + tanDown) / tanHeight;
        matrix[13] = 0;

        if (far_ <= near_) {
            matrix[2] = 0;
            matrix[6] = 0;
            matrix[10] = -1;
            matrix[14] = -(near_ + offset);
        }
        else {
            matrix[2] = 0;
            matrix[6] = 0;
            matrix[10] = -(far_ + offset) / (far_ - near_);
            matrix[14] = -(far_ * (near_ + offset)) / (far_ - near_);
        }

        matrix[3] = 0;
        matrix[7] = 0;
        matrix[11] = -1;
        matrix[15] = 0;

        return osg::Matrix(matrix);
    }
}

std::ostream& operator <<(
    std::ostream& os,
    const MWVR::Pose& pose)
{
    os << "position=" << pose.position << " orientation=" << pose.orientation << " velocity=" << pose.velocity;
    return os;
}

