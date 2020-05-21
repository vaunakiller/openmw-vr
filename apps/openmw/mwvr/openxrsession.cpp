#include "vrenvironment.hpp"
#include "openxrmanager.hpp"
#include "openxrmanagerimpl.hpp"
#include "openxrinputmanager.hpp"
#include "openxrswapchain.hpp"
#include "../mwinput/inputmanagerimp.hpp"
#include "../mwbase/environment.hpp"

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
#include "openxrsession.hpp"
#include "vrgui.hpp"
#include <time.h>

namespace MWVR
{
OpenXRSession::OpenXRSession()
{
}

OpenXRSession::~OpenXRSession()
{
}

void OpenXRSession::setLayer(
    OpenXRLayerStack::Layer layerType,
    OpenXRLayer* layer)
{
    mLayerStack.setLayer(layerType, layer);
}

void OpenXRSession::swapBuffers(osg::GraphicsContext* gc)
{
    Timer timer("OpenXRSession::SwapBuffers");

    auto* xr = Environment::get().getManager();

    if (!mShouldRender)
        return;

    if (mRenderedFrames != mRenderFrame - 1)
    {
        Log(Debug::Warning) << "swapBuffers called out of order";
        waitFrame();
        return;
    }

    for (auto layer : mLayerStack.layerObjects())
        if (layer)
            layer->swapBuffers(gc);

    timer.checkpoint("Rendered");
    xr->endFrame(mDrawPredictedDisplayTime, &mLayerStack);
    mRenderedFrames++;
}

const PoseSets& OpenXRSession::predictedPoses(PredictionSlice slice)
{
    if (slice == PredictionSlice::Predraw)
        waitFrame();
    return mPredictedPoses[(int)slice];
}

static std::chrono::steady_clock::time_point gLast{};

void OpenXRSession::waitFrame()
{
    if (mPredictedFrames < mPredictionFrame)
    {
        Timer timer("OpenXRSession::waitFrame");
        auto* xr = Environment::get().getManager();
        xr->handleEvents();
        mIsRunning = xr->sessionRunning();
        if (!mIsRunning)
            return;

        // Until OpenXR allows us to get a prediction without waiting
        // we make our own (bad) prediction and let openxr wait asynchronously.
        auto frameState = xr->impl().frameState();

        std::thread waitThread([]{
        Environment::get().getManager()->waitFrame();
        });

        if (mPredrawPredictedDisplayTime == 0)
        {
            // First time
            waitThread.join();
            mPredrawPredictedDisplayTime = xr->impl().frameState().predictedDisplayTime;
        }
        else
        {
            waitThread.detach();
            mPredrawPredictedDisplayTime = frameState.predictedDisplayTime + frameState.predictedDisplayPeriod;
        }

        timer.checkpoint("waitFrame");
        predictNext(0);
        mPredictedFrames++;
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

void OpenXRSession::movementAngles(float& yaw, float& pitch)
{
    auto lhandquat = predictedPoses(PredictionSlice::Predraw).hands[(int)TrackedSpace::VIEW][(int)MWVR::Side::LEFT_HAND].orientation;
    float roll = 0.f;
    getEulerAngles(lhandquat, yaw, pitch, roll);

}

void OpenXRSession::advanceFrame(void)
{
    auto* xr = Environment::get().getManager();
    mShouldRender = mIsRunning;
    if (!mIsRunning)
        return;
    if (mPredictedFrames != mPredictionFrame)
    {
        Log(Debug::Warning) << "advanceFrame() called out of order";
        return;
    }

    // For the same reason we do waitFrame() asynchronously,
    // we do beginFrame async and just blit to the swapchains later.
    //std::thread([=] {
    xr->beginFrame();
    //}).detach();
    mPredictionFrame++;
    mRenderFrame++;
    mPredictedPoses[(int)PredictionSlice::Draw] = mPredictedPoses[(int)PredictionSlice::Predraw];
    mDrawPredictedDisplayTime = mPredrawPredictedDisplayTime;
}

void OpenXRSession::predictNext(int extraPeriods)
{
    auto* xr = Environment::get().getManager();
    auto* input = Environment::get().getInputManager();

    PoseSets newPoses{};
    newPoses.head[(int)TrackedSpace::STAGE] = xr->impl().getPredictedLimbPose(mPredrawPredictedDisplayTime, TrackedLimb::HEAD, TrackedSpace::STAGE);
    newPoses.head[(int)TrackedSpace::VIEW] = xr->impl().getPredictedLimbPose(mPredrawPredictedDisplayTime, TrackedLimb::HEAD, TrackedSpace::VIEW);
    newPoses.hands[(int)TrackedSpace::STAGE] = input->getHandPoses(mPredrawPredictedDisplayTime, TrackedSpace::STAGE);
    newPoses.hands[(int)TrackedSpace::VIEW] = input->getHandPoses(mPredrawPredictedDisplayTime, TrackedSpace::VIEW);
    auto stageViews = xr->impl().getPredictedViews(mPredrawPredictedDisplayTime, TrackedSpace::STAGE);
    auto hmdViews = xr->impl().getPredictedViews(mPredrawPredictedDisplayTime, TrackedSpace::VIEW);
    newPoses.eye[(int)TrackedSpace::STAGE][(int)Side::LEFT_HAND] = fromXR(stageViews[(int)Side::LEFT_HAND].pose);
    newPoses.eye[(int)TrackedSpace::VIEW][(int)Side::LEFT_HAND] = fromXR(hmdViews[(int)Side::LEFT_HAND].pose);
    newPoses.eye[(int)TrackedSpace::STAGE][(int)Side::RIGHT_HAND] = fromXR(stageViews[(int)Side::RIGHT_HAND].pose);
    newPoses.eye[(int)TrackedSpace::VIEW][(int)Side::RIGHT_HAND] = fromXR(hmdViews[(int)Side::RIGHT_HAND].pose);
    mPredictedPoses[(int)PredictionSlice::Predraw] = newPoses;
}
}

std::ostream& operator <<(
    std::ostream& os,
    const MWVR::Pose& pose)
{
    os << "position=" << pose.position << " orientation=" << pose.orientation << " velocity=" << pose.velocity;
    return os;
}

