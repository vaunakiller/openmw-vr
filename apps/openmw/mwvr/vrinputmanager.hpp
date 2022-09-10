#ifndef VR_INPUT_MANAGER_HPP
#define VR_INPUT_MANAGER_HPP

#include "../mwinput/inputmanagerimp.hpp"

#include <components/vr/trackingpath.hpp>
#include <vector>
#include <array>
#include <iostream>

#include "../mwworld/ptr.hpp"

namespace XR
{
    class ActionSet;
    class InputAction;
}

namespace MWVR
{
    class OpenXRInput;
    class UserPointer;

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
            const std::string& controllerBindingsFile, bool grab,
            const std::string& xrControllerSuggestionsFile);

        virtual ~VRInputManager();

        static VRInputManager& instance();

        /// Overriden to force vr modes such as hiding cursors and crosshairs
        void changeInputMode(bool guiMode) override;

        /// Overriden to update XR inputs
        void update(float dt, bool disableControls = false, bool disableEvents = false) override;

        /// Currently active action set
        XR::ActionSet& activeActionSet();

        /// OpenXR input interface
        OpenXRInput& xrInput() { return *mXRInput; }

        void calibrate();
        void calibratePlayerHeight();

        UserPointer* vrPointer() { return mVRPointer.get(); }
        const UserPointer* vrPointer() const { return mVRPointer.get(); }

        osg::Node* vrAimNode() { return mVRAimNode; }
        const osg::Node* vrAimNode() const { return mVRAimNode; }

        void turnLeftRight(float value, float previousValue, float dt);

    protected:
        void processAction(const class XR::InputAction* action, float dt, bool disableControls);

        void updateVRPointer(bool disableControls);
        void updateCombat(float dt);
        void updateRealisticCombat(float dt);
        void pointActivation(bool onPress);

        void injectMousePress(int sdlButton, bool onPress);
        void injectChannelValue(MWInput::Actions action, float value);

        void applyHapticsLeftHand(float intensity) override;
        void applyHapticsRightHand(float intensity) override;
        void processChangedSettings(const std::set< std::pair<std::string, std::string> >& changed) override;

        void setThumbstickDeadzone(float deadzoneRadius);

        float smoothTurnRate(float dt) const;

    private:
        std::unique_ptr<UserPointer> mVRPointer;
        std::unique_ptr<OpenXRInput> mXRInput;
        std::unique_ptr<RealisticCombat::StateMachine> mRealisticCombat;
        bool mPointerLeft = false;
        bool mPointerRight = false;
        bool mHapticsEnabled = true;
        bool mSmoothTurning = true;
        float mSnapAngle = 30.f;
        float mSmoothTurnRate = 1.0f;

        osg::ref_ptr<osg::Node> mVRAimNode;

        VR::VRPath mLeftHandPath;
        VR::VRPath mLeftHandWorldPath;
        VR::VRPath mRightHandPath;
        VR::VRPath mRightHandWorldPath;
        VR::VRPath mHeadWorldPath;

        enum class CalibrationState
        {
            None = 0,
            Active = 1,
            Aborted = 2,
            Complete = 3,
        };

        CalibrationState mCalibrationState = CalibrationState::None;

    };
}

#endif
