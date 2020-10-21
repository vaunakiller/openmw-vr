#ifndef VR_INPUT_MANAGER_HPP
#define VR_INPUT_MANAGER_HPP

#include "vrtypes.hpp"

#include "../mwinput/inputmanagerimp.hpp"

#include <vector>
#include <array>
#include <iostream>

#include "../mwworld/ptr.hpp"

namespace MWVR
{
    struct OpenXRInput;
    struct OpenXRActionSet;

    namespace RealisticCombat {
        class StateMachine;
    }

    /// Extension of the input manager to include VR inputs
    class VRInputManager : public MWInput::InputManager
    {
    public:
        VRInputManager(
            SDL_Window* window,
            osg::ref_ptr<osgViewer::Viewer> viewer,
            osg::ref_ptr<osgViewer::ScreenCaptureHandler> screenCaptureHandler,
            osgViewer::ScreenCaptureHandler::CaptureOperation* screenCaptureOperation,
            const std::string& userFile, bool userFileExists,
            const std::string& userControllerBindingsFile,
            const std::string& controllerBindingsFile, bool grab);

        virtual ~VRInputManager();

        /// Overriden to force vr modes such as hiding cursors and crosshairs
        virtual void changeInputMode(bool guiMode);

        /// Overriden to update XR inputs
        virtual void update(float dt, bool disableControls = false, bool disableEvents = false);

        /// Set current offset to 0 and re-align VR stage.
        void requestRecenter();

        /// Tracking pose of the given limb at the given predicted time
        Pose getLimbPose(int64_t time, TrackedLimb limb);

        /// Currently active action set
        OpenXRActionSet& activeActionSet();

    protected:
        void processAction(const class Action* action, float dt, bool disableControls);

        void updateActivationIndication(void);
        void pointActivation(bool onPress);

        void injectMousePress(int sdlButton, bool onPress);
        void injectChannelValue(MWInput::Actions action, float value);

        void applyHapticsLeftHand(float intensity) override;
        void applyHapticsRightHand(float intensity) override;

    private:
        void suggestBindingsSimple();
        void suggestBindingsOculusTouch();
        void suggestBindingsHpMixedReality();
        void suggestBindingsMicrosoftMixedReality();
        void suggestBindingsIndex();
        void suggestBindingsVive();
        void suggestBindingsXboxController();

        std::unique_ptr<OpenXRInput> mXRInput;
        std::unique_ptr<RealisticCombat::StateMachine> mRealisticCombat;
        bool mActivationIndication{ false };
        bool mHapticsEnabled{ true };
    };
}

#endif
