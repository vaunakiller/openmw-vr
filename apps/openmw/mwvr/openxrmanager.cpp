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
            return mPrivate->frameIndex;
    }

    bool OpenXRManager::sessionRunning()
    {
        if (realized())
            return mPrivate->mSessionRunning;
        return false;
    }

    void OpenXRManager::handleEvents()
    {
        if (realized())
            return mPrivate->handleEvents();
    }

    void OpenXRManager::waitFrame()
    {
        if (realized())
            return mPrivate->waitFrame();
    }

    void OpenXRManager::beginFrame()
    {
        if (realized())
            return mPrivate->beginFrame();
    }

    void OpenXRManager::endFrame()
    {
        if (realized())
            return mPrivate->endFrame();
    }

    void OpenXRManager::updateControls()
    {
        if (realized())
            return mPrivate->updateControls();
    }

    void OpenXRManager::updatePoses()
    {
        if (realized())
            return mPrivate->updatePoses();
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

    void OpenXRManager::setPoseUpdateCallback(PoseUpdateCallback::TrackedLimb limb, PoseUpdateCallback::TrackingMode mode, osg::ref_ptr<PoseUpdateCallback> cb)
    {
        if (realized())
            return mPrivate->setPoseUpdateCallback(limb, mode, cb);
    }

    void OpenXRManager::setViewSubImage(int eye, const ::XrSwapchainSubImage& subImage)
    {
        if (realized())
            return mPrivate->setViewSubImage(eye, subImage);
    }

    int OpenXRManager::eyes()
    {
        if (realized())
            return mPrivate->eyes();
        return 0;
    }

    void
        OpenXRManager::RealizeOperation::operator()(
            osg::GraphicsContext* gc)
    {
        osg::ref_ptr<OpenXRManager> XR;
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

    void
        OpenXRManager::SwapBuffersCallback::swapBuffersImplementation(
            osg::GraphicsContext* gc)
    {
        gc->swapBuffersImplementation();
        mXR->endFrame();
    }
}
