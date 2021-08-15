#include "vrsession.hpp"
#include "vrgui.hpp"
#include "vrenvironment.hpp"
#include "vrinputmanager.hpp"
#include "openxrmanager.hpp"
#include "openxrswapchain.hpp"
#include "../mwinput/inputmanagerimp.hpp"
#include "../mwbase/environment.hpp"
#include "../mwbase/statemanager.hpp"

#include <components/debug/debuglog.hpp>
#include <components/sdlutil/sdlgraphicswindow.hpp>
#include <components/misc/stringops.hpp>
#include <components/misc/constants.hpp>

#include <osg/Camera>

#include <algorithm>
#include <vector>
#include <array>
#include <iostream>
#include <time.h>
#include <thread>
#include <chrono>

#ifdef max
#undef max
#endif

#ifdef min
#undef min
#endif

namespace MWVR
{
    VRSession::VRSession()
    {
        mSeatedPlay = Settings::Manager::getBool("seated play", "VR");
    }

    VRSession::~VRSession()
    {
    }

    void VRSession::processChangedSettings(const std::set<std::pair<std::string, std::string>>& changed)
    {
        setSeatedPlay(Settings::Manager::getBool("seated play", "VR"));
    }

    VR::Frame VRSession::newFrame()
    {
        static uint64_t frameNo = 0;
        VR::Frame frame;
        frame.frameNumber = frameNo++;
        return frame;
    }

    void VRSession::frameBeginUpdate(VR::Frame& frame)
    {
        mFrames++;

        auto* xr = Environment::get().getManager();
        xr->handleEvents();

        frame.shouldSyncFrameLoop = xr->appShouldSyncFrameLoop();
        if (frame.shouldSyncFrameLoop)
        {
            auto info = xr->waitFrame();
            frame.runtimePredictedDisplayTime = info.runtimePredictedDisplayTime;
            frame.runtimePredictedDisplayPeriod = info.runtimePredictedDisplayPeriod;
            frame.shouldRender = info.runtimeRequestsRender;
        }
    }

    void VRSession::frameBeginRender(VR::Frame& frame)
    {
        if(frame.shouldSyncFrameLoop)
            Environment::get().getManager()->beginFrame();
    }

    void VRSession::frameEnd(osg::GraphicsContext* gc, VRViewer& viewer, VR::Frame& frame)
    {
        auto* xr = Environment::get().getManager();

        gc->swapBuffersImplementation();
        if (frame.shouldSyncFrameLoop)
        {
            xr->endFrame(frame);
        }
    }

    void VRSession::setSeatedPlay(bool seatedPlay)
    {
        std::swap(mSeatedPlay, seatedPlay);
        if (mSeatedPlay != seatedPlay)
        {
            Environment::get().getInputManager()->requestRecenter(true);
        }
    }

    // OSG doesn't provide API to extract euler angles from a quat, but i need it.
    // Credits goes to Dennis Bunfield, i just copied his formula https://narkive.com/v0re6547.4
    void getEulerAngles(const osg::Quat& quat, float& yaw, float& pitch, float& roll)
    {
        // Now do the computation
        osg::Matrixd m2(osg::Matrixd::rotate(quat));
        double* mat = (double*)m2.ptr();
        double angle_x = 0.0;
        double angle_y = 0.0;
        double angle_z = 0.0;
        double D, C, tr_x, tr_y;
        angle_y = D = asin(mat[2]); /* Calculate Y-axis angle */
        C = cos(angle_y);
        if (fabs(C) > 0.005) /* Test for Gimball lock? */
        {
            tr_x = mat[10] / C; /* No, so get X-axis angle */
            tr_y = -mat[6] / C;
            angle_x = atan2(tr_y, tr_x);
            tr_x = mat[0] / C; /* Get Z-axis angle */
            tr_y = -mat[1] / C;
            angle_z = atan2(tr_y, tr_x);
        }
        else /* Gimball lock has occurred */
        {
            angle_x = 0; /* Set X-axis angle to zero
            */
            tr_x = mat[5]; /* And calculate Z-axis angle
            */
            tr_y = mat[4];
            angle_z = atan2(tr_y, tr_x);
        }

        yaw = angle_z;
        pitch = angle_x;
        roll = angle_y;
    }
}

