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
#include <iostream>

namespace MWVR
{


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

    void OpenXRManager::beginFrame(long long frameIndex)
    {
        if (realized())
            return impl().beginFrame(frameIndex);
    }

    void OpenXRManager::endFrame()
    {
        if (realized())
            return impl().endFrame();
    }

    void OpenXRManager::updateControls()
    {
        if (realized())
            return impl().updateControls();
    }

    void OpenXRManager::updatePoses()
    {
        if (realized())
            return impl().updatePoses();
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
            catch (std::exception& e)
            {
                Log(Debug::Error) << "Exception thrown by OpenXR: " << e.what();
                osg::ref_ptr<osg::State> state = gc->getState();
                
            }
        }
    }


    void OpenXRManager::addPoseUpdateCallback(
        osg::ref_ptr<PoseUpdateCallback> cb)
    {
        if (realized())
            return impl().addPoseUpdateCallback(cb);
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
        mXR->realize(gc);
    }

    bool 
        OpenXRManager::RealizeOperation::realized()
    {
        return mXR->realized();
    }

    void 
        OpenXRManager::CleanupOperation::operator()(
            osg::GraphicsContext* gc)
    {

    }

    void OpenXRManager::viewerBarrier()
    {
        if (realized())
            return impl().viewerBarrier();
    }

    void OpenXRManager::registerToBarrier()
    {
        if (realized())
            return impl().registerToBarrier();
    }

    void OpenXRManager::unregisterFromBarrier()
    {
        if (realized())
            return impl().unregisterFromBarrier();
    }

#ifndef _NDEBUG
    void PoseLogger::operator()(MWVR::Pose pose)
    {
        const char* limb = nullptr;
        const char* space = nullptr;
        switch (mLimb)
        {
        case TrackedLimb::HEAD:
            limb = "HEAD"; break;
        case TrackedLimb::LEFT_HAND:
            limb = "LEFT_HAND"; break;
        case TrackedLimb::RIGHT_HAND:
            limb = "RIGHT_HAND"; break;
        }
        switch (mSpace)
        {
        case TrackedSpace::STAGE:
            space = "STAGE"; break;
        case TrackedSpace::VIEW:
            space = "VIEW"; break;
        }

        //TODO: Use a different output to avoid spamming the debug log when enabled
        Log(Debug::Verbose) << space << "." << limb << ": " << pose;
    }
#endif
}

std::ostream& operator <<(
    std::ostream& os,
    const MWVR::Pose& pose)
{
    os << "position=" << pose.position << " orientation=" << pose.orientation << " velocity=" << pose.velocity;
    return os;
}

