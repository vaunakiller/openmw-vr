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
#include "openxrmenu.hpp"
#include <time.h>

namespace MWVR
{
    OpenXRSession::OpenXRSession(
        osg::ref_ptr<OpenXRManager> XR)
        : mXR(XR)
        , mInputManager(new OpenXRInputManager(mXR))
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
        mXR->handleEvents();
        if (!mXR->sessionRunning())
            return;

        // For now it seems we must just accept crap performance from the rendering loop
        // Since Oculus' implementation of waitFrame() does not even attempt to reflect real
        // render time and just incurs a huge useless delay.
        Timer timer("OpenXRSession::waitFrame");
        mXR->waitFrame();
        timer.checkpoint("waitFrame");
        mInputManager->updateControls();
        predictNext(0);

        OpenXRActionEvent event{};
        while (mInputManager->nextActionEvent(event))
        {
            Log(Debug::Verbose) << "ActionEvent action=" << event.action << " onPress=" << event.onPress;
            if (event.action == MWInput::InputManager::A_GameMenu)
            {
                Log(Debug::Verbose) << "A_GameMenu";
                auto* menuLayer = dynamic_cast<OpenXRMenu*>(mLayerStack.layerObjects()[OpenXRLayerStack::MENU_VIEW_LAYER]);
                if (menuLayer)
                {
                    menuLayer->setVisible(!menuLayer->isVisible());
                    menuLayer->updatePosition();
                }
            }
        }

        mPredictionsReady = true;

    }

    void OpenXRSession::predictNext(int extraPeriods)
    {
        auto mPredictedDisplayTime = mXR->impl().frameState().predictedDisplayTime;


        // Update pose predictions
        mPredictedPoses.head[(int)TrackedSpace::STAGE] = mXR->impl().getPredictedLimbPose(mPredictedDisplayTime, TrackedLimb::HEAD, TrackedSpace::STAGE);
        mPredictedPoses.head[(int)TrackedSpace::VIEW] = mXR->impl().getPredictedLimbPose(mPredictedDisplayTime, TrackedLimb::HEAD, TrackedSpace::VIEW);
        mPredictedPoses.hands[(int)TrackedSpace::STAGE] = mInputManager->getHandPoses(mPredictedDisplayTime, TrackedSpace::STAGE);
        mPredictedPoses.hands[(int)TrackedSpace::VIEW] = mInputManager->getHandPoses(mPredictedDisplayTime, TrackedSpace::VIEW);
        auto stageViews = mXR->impl().getPredictedViews(mPredictedDisplayTime, TrackedSpace::STAGE);
        auto hmdViews = mXR->impl().getPredictedViews(mPredictedDisplayTime, TrackedSpace::VIEW);
        mPredictedPoses.eye[(int)TrackedSpace::STAGE][(int)Chirality::LEFT_HAND] = fromXR(stageViews[(int)Chirality::LEFT_HAND].pose);
        mPredictedPoses.eye[(int)TrackedSpace::VIEW][(int)Chirality::LEFT_HAND] = fromXR(hmdViews[(int)Chirality::LEFT_HAND].pose);
        mPredictedPoses.eye[(int)TrackedSpace::STAGE][(int)Chirality::RIGHT_HAND] = fromXR(stageViews[(int)Chirality::RIGHT_HAND].pose);
        mPredictedPoses.eye[(int)TrackedSpace::VIEW][(int)Chirality::RIGHT_HAND] = fromXR(hmdViews[(int)Chirality::RIGHT_HAND].pose);
    }
}

std::ostream& operator <<(
    std::ostream& os,
    const MWVR::Pose& pose)
{
    os << "position=" << pose.position << " orientation=" << pose.orientation << " velocity=" << pose.velocity;
    return os;
}

