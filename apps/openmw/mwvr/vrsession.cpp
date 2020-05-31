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
#include "vrsession.hpp"
#include "vrgui.hpp"
#include <time.h>

namespace MWVR
{
VRSession::VRSession()
{
}

VRSession::~VRSession()
{
}

osg::Matrix VRSession::projectionMatrix(FramePhase phase, Side side)
{
    assert(((int)side) < 2);
    auto fov = predictedPoses(VRSession::FramePhase::Predraw).view[(int)side].fov;
    float near_ = Settings::Manager::getFloat("near clip", "Camera");
    float far_ = Settings::Manager::getFloat("viewing distance", "Camera");
    return fov.perspectiveMatrix(near_, far_);
}

osg::Matrix VRSession::viewMatrix(FramePhase phase, Side side)
{
    MWVR::Pose pose = predictedPoses(VRSession::FramePhase::Predraw).view[(int)side].pose;
    osg::Vec3 position = pose.position * Environment::get().unitsPerMeter();
    osg::Quat orientation = pose.orientation;

    float y = position.y();
    float z = position.z();
    position.y() = z;
    position.z() = -y;

    y = orientation.y();
    z = orientation.z();
    orientation.y() = z;
    orientation.z() = -y;

    osg::Matrix viewMatrix;
    viewMatrix.setTrans(-position);
    viewMatrix.postMultRotate(orientation.conj());

    return viewMatrix;
}

bool VRSession::isRunning() const {
    auto* xr = Environment::get().getManager();
    return xr->sessionRunning();
}

void VRSession::swapBuffers(osg::GraphicsContext* gc, VRViewer& viewer)
{
    Timer timer("VRSession::SwapBuffers");
    auto* xr = Environment::get().getManager();

    {
        std::unique_lock<std::mutex> lock(mMutex); 

        xr->handleEvents();

        mPostdrawFrame = std::move(mDrawFrame);
        assert(mPostdrawFrame);
    }

    // TODO: Should blit mirror texture regardless
    if (!isRunning())
        return;

    xr->waitFrame();
    xr->beginFrame();
    viewer.swapBuffers(gc);

    auto leftView = viewer.mViews["LeftEye"];
    auto rightView = viewer.mViews["RightEye"];

    leftView->swapBuffers(gc);
    rightView->swapBuffers(gc);

    std::array<XrCompositionLayerProjectionView, 2> compositionLayerProjectionViews{};
    compositionLayerProjectionViews[0].type = XR_TYPE_COMPOSITION_LAYER_PROJECTION_VIEW;
    compositionLayerProjectionViews[1].type = XR_TYPE_COMPOSITION_LAYER_PROJECTION_VIEW;
    compositionLayerProjectionViews[0].subImage = leftView->swapchain().subImage();
    compositionLayerProjectionViews[1].subImage = rightView->swapchain().subImage();
    compositionLayerProjectionViews[0].pose = toXR(mPostdrawFrame->mPredictedPoses.eye[(int)Side::LEFT_HAND]);
    compositionLayerProjectionViews[1].pose = toXR(mPostdrawFrame->mPredictedPoses.eye[(int)Side::RIGHT_HAND]);
    compositionLayerProjectionViews[0].fov = toXR(mPostdrawFrame->mPredictedPoses.view[(int)Side::LEFT_HAND].fov);
    compositionLayerProjectionViews[1].fov = toXR(mPostdrawFrame->mPredictedPoses.view[(int)Side::RIGHT_HAND].fov);
    XrCompositionLayerProjection layer{};
    layer.type = XR_TYPE_COMPOSITION_LAYER_PROJECTION;
    layer.space = xr->impl().getReferenceSpace(TrackedSpace::STAGE);
    layer.viewCount = 2;
    layer.views = compositionLayerProjectionViews.data();
    auto* layerStack = reinterpret_cast<XrCompositionLayerBaseHeader*>(&layer);

    xr->endFrame(mPostdrawFrame->mPredictedDisplayTime, 1, &layerStack);
    mLastRenderedFrame = mPostdrawFrame->mFrameNo;
    auto now = std::chrono::steady_clock::now();
    mLastFrameInterval = std::chrono::duration_cast<std::chrono::nanoseconds>(now - mLastRenderedFrameTimestamp);
    mLastRenderedFrameTimestamp = now;
}

void VRSession::advanceFramePhase(void)
{
    std::unique_lock<std::mutex> lock(mMutex);
    Timer timer("VRSession::advanceFrame");

    assert(!mDrawFrame);
    mDrawFrame = std::move(mPredrawFrame);
}

void VRSession::startFrame()
{
    std::unique_lock<std::mutex> lock(mMutex);
    assert(!mPredrawFrame);

    Timer timer("VRSession::startFrame");
    auto* xr = Environment::get().getManager();
    auto* input = Environment::get().getInputManager();

    // Until OpenXR allows us to get a prediction without waiting
    // we make our own (bad) prediction and let openxr wait
    auto frameState = xr->impl().frameState();
    long long predictedDisplayTime = 0;
    mFrames++;
    if (mLastPredictedDisplayTime == 0)
    {
        // First time, need to invent a frame time since openxr won't help us without calling waitframe.
        predictedDisplayTime = std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
    }
    else if (mFrames > mLastRenderedFrame)
    {
        //predictedDisplayTime = mLastPredictedDisplayTime + mLastFrameInterval.count() * (mFrames - mLastRenderedFrame);
        float intervalsf = static_cast<double>(mLastFrameInterval.count()) / static_cast<double>(frameState.predictedDisplayPeriod);
        int intervals = (int)std::roundf(intervalsf);
        predictedDisplayTime = mLastPredictedDisplayTime + intervals * (mFrames - mLastRenderedFrame) * frameState.predictedDisplayPeriod;
    }

    PoseSet predictedPoses{};
    if (isRunning())
    {
        predictedPoses.head = xr->impl().getPredictedLimbPose(predictedDisplayTime, TrackedLimb::HEAD, TrackedSpace::STAGE) * mPlayerScale;
        predictedPoses.hands[(int)Side::LEFT_HAND] = input->getHandPose(predictedDisplayTime, TrackedSpace::STAGE, Side::LEFT_HAND) * mPlayerScale;
        predictedPoses.hands[(int)Side::RIGHT_HAND] = input->getHandPose(predictedDisplayTime, TrackedSpace::STAGE, Side::RIGHT_HAND) * mPlayerScale;
        auto hmdViews = xr->impl().getPredictedViews(predictedDisplayTime, TrackedSpace::VIEW);
        predictedPoses.view[(int)Side::LEFT_HAND].pose = fromXR(hmdViews[(int)Side::LEFT_HAND].pose) * mPlayerScale;
        predictedPoses.view[(int)Side::RIGHT_HAND].pose = fromXR(hmdViews[(int)Side::RIGHT_HAND].pose) * mPlayerScale;
        predictedPoses.view[(int)Side::LEFT_HAND].fov = fromXR(hmdViews[(int)Side::LEFT_HAND].fov);
        predictedPoses.view[(int)Side::RIGHT_HAND].fov = fromXR(hmdViews[(int)Side::RIGHT_HAND].fov);
        auto stageViews = xr->impl().getPredictedViews(predictedDisplayTime, TrackedSpace::STAGE);
        predictedPoses.eye[(int)Side::LEFT_HAND] = fromXR(stageViews[(int)Side::LEFT_HAND].pose) * mPlayerScale;
        predictedPoses.eye[(int)Side::RIGHT_HAND] = fromXR(stageViews[(int)Side::RIGHT_HAND].pose) * mPlayerScale;
    }
    else
    {
        // If session is not running, copy poses from the last frame
        if (mPostdrawFrame)
            predictedPoses = mPostdrawFrame->mPredictedPoses;
    }


    mPredrawFrame.reset(new VRFrame);
    mPredrawFrame->mPredictedDisplayTime = predictedDisplayTime;
    mPredrawFrame->mFrameNo = mFrames;
    mPredrawFrame->mPredictedPoses = predictedPoses;
}

const PoseSet& VRSession::predictedPoses(FramePhase phase)
{
    
    switch (phase)
    {
    case FramePhase::Predraw:
        if (!mPredrawFrame)
            startFrame();
        assert(mPredrawFrame);
        return mPredrawFrame->mPredictedPoses;

    case FramePhase::Draw:
        assert(mDrawFrame);
        return mDrawFrame->mPredictedPoses;

    case FramePhase::Postdraw:
        assert(mPostdrawFrame);
        return mPostdrawFrame->mPredictedPoses;
    }

    assert(0);
    return mPredrawFrame->mPredictedPoses;
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

void VRSession::movementAngles(float& yaw, float& pitch)
{
    assert(mPredrawFrame);
    auto* input = Environment::get().getInputManager();
    auto lhandquat = input->getHandPose(mPredrawFrame->mPredictedDisplayTime, TrackedSpace::VIEW, Side::LEFT_HAND).orientation;
        //predictedPoses(FramePhase::Predraw).hands[(int)TrackedSpace::VIEW][(int)MWVR::Side::LEFT_HAND].orientation;
    float roll = 0.f;
    getEulerAngles(lhandquat, yaw, pitch, roll);

}

}

std::ostream& operator <<(
    std::ostream& os,
    const MWVR::Pose& pose)
{
    os << "position=" << pose.position << " orientation=" << pose.orientation << " velocity=" << pose.velocity;
    return os;
}

