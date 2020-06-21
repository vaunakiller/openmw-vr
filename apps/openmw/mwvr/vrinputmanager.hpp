#ifndef VR_INPUT_MANAGER_HPP
#define VR_INPUT_MANAGER_HPP

#include "vrviewer.hpp"
#include "realisticcombat.hpp"
#include "../mwinput/inputmanagerimp.hpp"

#include <vector>
#include <array>
#include <iostream>

#include "../mwworld/ptr.hpp"

namespace MWVR
{
struct OpenXRInput;

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

    /// Overriden to force vr modes such as hiding cursors and crosshairs
    virtual void changeInputMode(bool guiMode);

    /// Overriden to update XR inputs
    virtual void update(float dt, bool disableControls = false, bool disableEvents = false);

    /// Current head offset from character position
    osg::Vec3 headOffset() const { return mHeadOffset; };

    /// Update head offset. Should only be called by the movement solver when reducing head offset.
    void setHeadOffset(osg::Vec3 offset) { mHeadOffset = offset; };

    /// Quaternion that aligns VR stage coordinates with world coordinates.
    osg::Quat stageRotation();

    /// Set current offset to 0 and re-align VR stage.
    void requestRecenter();

    /// Tracking pose of the given limb at the given predicted time
    Pose getLimbPose(int64_t time, TrackedLimb limb);

protected:
    void updateHead();

    void processAction(const class Action* action, float dt, bool disableControls);

    void updateActivationIndication(void);
    void pointActivation(bool onPress);

    void injectMousePress(int sdlButton, bool onPress);
    void injectChannelValue(MWInput::Actions action, float value);

    void applyHapticsLeftHand(float intensity) override;
    void applyHapticsRightHand(float intensity) override;

    std::unique_ptr<OpenXRInput> mXRInput;
    std::unique_ptr<RealisticCombat::StateMachine> mRealisticCombat;
    Pose mHeadPose{};
    osg::Vec3 mHeadOffset{ 0,0,0 };
    bool mShouldRecenter{ true };
    bool mActivationIndication{ false };
    float mYaw{ 0.f };

    float mVrAngles[3]{ 0.f,0.f,0.f };
};
}

#endif
