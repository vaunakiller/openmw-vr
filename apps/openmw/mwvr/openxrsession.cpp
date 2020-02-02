#include "openxrmanager.hpp"
#include "openxrmanagerimpl.hpp"
#include "openxrswapchain.hpp"
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
#include "openxrsession.hpp"
#include <time.h>

namespace MWVR
{
    OpenXRSession::OpenXRSession(
        osg::ref_ptr<OpenXRManager> XR)
        : mXR(XR)
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
        Log(Debug::Verbose) << "OpenXRSession::swapBuffers";
        Timer timer("OpenXRSession::SwapBuffers");
        static int wat = 0;
        if (!mXR->sessionRunning())
            return;
        if (!mPredictionsReady)
            return;

        for (auto layer : mLayerStack.layerObjects())
            layer->swapBuffers(gc);

        timer.checkpoint("Rendered");

        mXR->endFrame(mXR->impl().frameState().predictedDisplayTime, &mLayerStack);
    }

    void OpenXRSession::waitFrame()
    {
        // For now it seems we must just accept crap performance from the rendering loop
        // Since Oculus' implementation of waitFrame() does not even attempt to reflect real
        // render time and just incurs a huge useless delay.
        Timer timer("OpenXRSession::waitFrame");
        mXR->waitFrame();
        timer.checkpoint("waitFrame");
        predictNext(0);
        mPredictionsReady = true;

    }

    void OpenXRSession::predictNext(int extraPeriods)
    {
        auto mPredictedDisplayTime = mXR->impl().frameState().predictedDisplayTime;

        // Update pose predictions
        mPredictedPoses.head[(int)TrackedSpace::STAGE] = mXR->impl().getPredictedLimbPose(mPredictedDisplayTime, TrackedLimb::HEAD, TrackedSpace::STAGE);
        mPredictedPoses.head[(int)TrackedSpace::VIEW] = mXR->impl().getPredictedLimbPose(mPredictedDisplayTime, TrackedLimb::HEAD, TrackedSpace::VIEW);
        mPredictedPoses.hands[(int)Chirality::LEFT_HAND][(int)TrackedSpace::STAGE] = mXR->impl().getPredictedLimbPose(mPredictedDisplayTime, TrackedLimb::LEFT_HAND, TrackedSpace::STAGE);
        mPredictedPoses.hands[(int)Chirality::LEFT_HAND][(int)TrackedSpace::VIEW] = mXR->impl().getPredictedLimbPose(mPredictedDisplayTime, TrackedLimb::LEFT_HAND, TrackedSpace::VIEW);
        mPredictedPoses.hands[(int)Chirality::RIGHT_HAND][(int)TrackedSpace::STAGE] = mXR->impl().getPredictedLimbPose(mPredictedDisplayTime, TrackedLimb::RIGHT_HAND, TrackedSpace::STAGE);
        mPredictedPoses.hands[(int)Chirality::RIGHT_HAND][(int)TrackedSpace::VIEW] = mXR->impl().getPredictedLimbPose(mPredictedDisplayTime, TrackedLimb::RIGHT_HAND, TrackedSpace::VIEW);
        auto stageViews = mXR->impl().getPredictedViews(mPredictedDisplayTime, TrackedSpace::STAGE);
        auto hmdViews = mXR->impl().getPredictedViews(mPredictedDisplayTime, TrackedSpace::VIEW);
        mPredictedPoses.eye[(int)Chirality::LEFT_HAND][(int)TrackedSpace::STAGE] = fromXR(stageViews[(int)Chirality::LEFT_HAND].pose);
        mPredictedPoses.eye[(int)Chirality::LEFT_HAND][(int)TrackedSpace::VIEW] = fromXR(hmdViews[(int)Chirality::LEFT_HAND].pose);
        mPredictedPoses.eye[(int)Chirality::RIGHT_HAND][(int)TrackedSpace::STAGE] = fromXR(stageViews[(int)Chirality::RIGHT_HAND].pose);
        mPredictedPoses.eye[(int)Chirality::RIGHT_HAND][(int)TrackedSpace::VIEW] = fromXR(hmdViews[(int)Chirality::RIGHT_HAND].pose);
    }
}

std::ostream& operator <<(
    std::ostream& os,
    const MWVR::Pose& pose)
{
    os << "position=" << pose.position << " orientation=" << pose.orientation << " velocity=" << pose.velocity;
    return os;
}

