#ifndef OPENXR_INPUT_MANAGER_HPP
#define OPENXR_INPUT_MANAGER_HPP

#include "openxrviewer.hpp"
#include "realisticcombat.hpp"
#include "../mwinput/inputmanagerimp.hpp"

#include <vector>
#include <array>
#include <iostream>

#include "../mwworld/ptr.hpp"

namespace MWVR
{
    struct OpenXRInput;
    struct OpenXRActionEvent;

    /// As far as I can tell, SDL does not support VR controllers.
    /// So I subclass the input manager and insert VR controls.
    class OpenXRInputManager : public MWInput::InputManager
    {
    public:
        OpenXRInputManager(
            SDL_Window* window,
            osg::ref_ptr<osgViewer::Viewer> viewer,
            osg::ref_ptr<osgViewer::ScreenCaptureHandler> screenCaptureHandler,
            osgViewer::ScreenCaptureHandler::CaptureOperation* screenCaptureOperation,
            const std::string& userFile, bool userFileExists,
            const std::string& userControllerBindingsFile,
            const std::string& controllerBindingsFile, bool grab);

        virtual ~OpenXRInputManager();

        /// Overriden to always disallow mouselook and similar.
        virtual void changeInputMode(bool guiMode);

        /// Overriden to update XR inputs
        virtual void update(float dt, bool disableControls = false, bool disableEvents = false);

        void updateHead();

        void processEvent(const OpenXRActionEvent& event);

        PoseSet getHandPoses(int64_t time, TrackedSpace space);

        void updateActivationIndication(void);
        void pointActivation(bool onPress);

        void injectMousePress(int sdlButton, bool onPress);

        std::unique_ptr<OpenXRInput> mXRInput;
        std::unique_ptr<RealisticCombat::StateMachine> mRealisticCombat;
        Pose mPreviousHeadPose{};
        osg::Vec3 mHeadOffset{ 0,0,0 };
        bool mRecenter{ true };
        bool mActivationIndication{ false };
        float mYaw{ 0.f };

        float mVrAngles[3]{ 0.f,0.f,0.f };
    };
}

#endif
