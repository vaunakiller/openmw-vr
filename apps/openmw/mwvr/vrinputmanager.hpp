#ifndef VR_INPUT_MANAGER_HPP
#define VR_INPUT_MANAGER_HPP

#include "../mwinput/inputmanagerimp.hpp"

#include <vector>
#include <array>
#include <iostream>

#include "../mwworld/ptr.hpp"
#include <components/vr/trackinglistener.hpp>
#include <components/vr/trackingpath.hpp>

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
            const std::string& xrControllerSuggestionsFile,
            const std::string& defaultXrControllerSuggestionsFile);

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

        void mouseMove(float x);
        void turnLeftRight(float value, float previousValue, float dt);

        void processUtilityStickX(float value);
        void processUtilityStickY(float value);

    protected:
        void processAction(const class XR::InputAction* action, float dt, bool disableControls);
        void processMovementStick(const class XR::InputAction* action, float dt, bool disableControls);
        void toggleUtilityDown();
        void toggleUtilityUp();
        void onActivateAction(int actionId);
        void onDeactivateAction(int actionId);

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

        int interactiveMessageBox(const std::string& message, const std::vector<std::string>& buttons);
        void updatePhysicalSneak(Stereo::Unit headsetHeight);

    private:
        osg::observer_ptr<osgViewer::Viewer> mOSGViewer;
        std::unique_ptr<UserPointer> mVRPointer;
        std::unique_ptr<OpenXRInput> mXRInput;
        std::unique_ptr<RealisticCombat::StateMachine> mRealisticCombat;
        bool mPointerLeft = false;
        bool mPointerRight = false;
        bool mHapticsEnabled = true;
        bool mSmoothTurning = true;
        bool mIsToggleSneak = false;
        float mSnapAngle = 30.f;
        float mSmoothTurnRate = 1.0f;
        Stereo::Unit mPhysicalSneakHeightOffset;
        bool mPhysicalSneakEnabled = true;
        bool mUtilityDownActive = false;
        bool mUtilityUpActive = false;


        osg::ref_ptr<osg::Node> mVRAimNode;

        VR::VRPath mLeftHandPath;
        VR::VRPath mLeftHandWorldPath;
        VR::VRPath mRightHandPath;
        VR::VRPath mRightHandWorldPath;
        VR::VRPath mHeadWorldPath;

        class HeightUpdateListener : public VR::TrackingListener
        {
            VR::VRPath mHeadPath = VR::stringToVRPath("/stage/user/head/input/pose");

            void onTrackingUpdated(VR::TrackingManager& manager, VR::DisplayTime predictedDisplayTime) override;
        };

        HeightUpdateListener mHeightUpdateListener;

    };
}

#endif
