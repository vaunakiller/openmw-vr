#include "vranimation.hpp"
#include "openxrinputmanager.hpp"
#include "vrenvironment.hpp"
#include "openxrmanager.hpp"
#include "openxrmanagerimpl.hpp"

#include <components/debug/debuglog.hpp>
#include <components/sdlutil/sdlgraphicswindow.hpp>

#include <MyGUI_InputManager.h>
#include <MyGUI_RenderManager.h>
#include <MyGUI_Widget.h>
#include <MyGUI_Button.h>
#include <MyGUI_EditBox.h>

#include <components/debug/debuglog.hpp>
#include <components/sdlutil/sdlinputwrapper.hpp>
#include <components/sdlutil/sdlvideowrapper.hpp>
#include <components/esm/esmwriter.hpp>
#include <components/esm/esmreader.hpp>
#include <components/esm/controlsstate.hpp>

#include "../mwbase/world.hpp"
#include "../mwbase/windowmanager.hpp"
#include "../mwbase/statemanager.hpp"
#include "../mwbase/environment.hpp"
#include "../mwbase/mechanicsmanager.hpp"

#include "../mwgui/itemmodel.hpp"
#include "../mwgui/draganddrop.hpp"

#include "../mwinput/actionmanager.hpp"
#include "../mwinput/bindingsmanager.hpp"
#include "../mwinput/controlswitch.hpp"
#include "../mwinput/mousemanager.hpp"

#include "../mwworld/player.hpp"
#include "../mwworld/class.hpp"
#include "../mwworld/inventorystore.hpp"
#include "../mwworld/esmstore.hpp"
#include "../mwmechanics/actorutil.hpp"

#include "../mwrender/renderingmanager.hpp"
#include "../mwrender/camera.hpp"

#include <openxr/openxr.h>

#include <extern/oics/ICSInputControlSystem.h>

#include <osg/Camera>

#include <vector>
#include <deque>
#include <array>
#include <iostream>

namespace MWVR
{

// TODO: Make part of settings (is there already a setting like this?)
//! Delay before a long-press action is activated
static std::chrono::milliseconds gActionTime{ 1000 };
//! Magnitude above which an axis action is considered active
static float gAxisEpsilon{ 0.01f };

struct OpenXRAction
{
public:
    // I want these to manage XrAction objects but i also didn't want to wrap these in unique pointers (useless dynamic allocation).
    // So i implemented move for them instead.
    OpenXRAction() = delete;
    OpenXRAction(OpenXRAction&& rhs) { mAction = nullptr; *this = std::move(rhs); }
    void operator=(OpenXRAction&& rhs);
private:
    OpenXRAction(const OpenXRAction&) = default;
    OpenXRAction& operator=(const OpenXRAction&) = default;
public:
    OpenXRAction(XrAction action, XrActionType actionType, const std::string& actionName, const std::string& localName);
    ~OpenXRAction();

    //! Convenience / syntactic sugar
    operator XrAction() { return mAction; }

    // Note that although these take a subaction path, we never use it. We have no subaction-ambiguous actions.
    bool getFloat(XrPath subactionPath, float& value);
    bool getBool(XrPath subactionPath, bool& value);
    bool getPose(XrPath subactionPath);
    bool applyHaptics(XrPath subactionPath, float amplitude);

    XrAction mAction = XR_NULL_HANDLE;
    XrActionType mType;
    std::string mName;
    std::string mLocalName;
};

//! Generic action
class Action
{
public:
    Action(int openMWAction, OpenXRAction xrAction) : mXRAction(std::move(xrAction)), mOpenMWAction(openMWAction){}
    virtual ~Action() {};

    //! True if action changed to being released in the last update
    bool isActive() const { return mActive; };

    //! True if activation turned on this update (i.e. onPress)
    bool onActivate() const { return mOnActivate; }

    //! True if activation turned off this update (i.e. onRelease)
    bool onDeactivate() const { return mOnDeactivate; }

    //! OpenMW Action code of this action
    int openMWActionCode() const { return mOpenMWAction; }

    //! Current value of an axis or lever action
    float value() const { return mValue; }

    //! Update internal states. Note that subclasses maintain both mValue and mActivate to allow
    //! axis and press to subtitute one another.
    virtual void update() = 0;

    //! Determines if an action update should be queued
    virtual bool shouldQueue() const = 0;

    //! Convenience / syntactic sugar
    operator XrAction() { return mXRAction; }

    //! Update and queue action if applicable
    void updateAndQueue(std::deque<const Action*>& queue)
    {
        bool old = mActive;
        update();
        bool changed = old != mActive;
        mOnActivate = changed && mActive;
        mOnDeactivate = changed && !mActive;

        if (shouldQueue())
        {
            queue.push_back(this);
        }
    }

protected:

    OpenXRAction mXRAction;
    int mOpenMWAction;
    float mValue{ 0.f };
    bool mActive{ false };
    bool mOnActivate{ false };
    bool mOnDeactivate{ false };
};

//! Convenience
using ActionPtr = std::unique_ptr<Action>;

//! Action that activates once on release.
//! Times out if the button is held longer than gHoldDelay.
class ButtonPressAction : public Action
{
public:
    using Action::Action;

    static const XrActionType ActionType = XR_ACTION_TYPE_BOOLEAN_INPUT;

    void update() override
    {
        mActive = false;
        bool old = mPressed;
        mXRAction.getBool(0, mPressed);
        bool changed = old != mPressed;
        if (changed && mPressed)
        {
            mPressTime = std::chrono::steady_clock::now();
            mTimeout = mPressTime + gActionTime;
        }
        if (changed && !mPressed)
        {
            if (std::chrono::steady_clock::now() < mTimeout)
            {
                mActive = true;
            }
        }

        mValue = mPressed ? 1.f : 0.f;
    }

    virtual bool shouldQueue() const override { return onActivate() || onDeactivate(); }

    bool mPressed{ false };
    std::chrono::steady_clock::time_point mPressTime{};
    std::chrono::steady_clock::time_point mTimeout{};
};

//! Action that activates once on a long press
//! The press time is the same as the timeout for a regular press, allowing keys with double roles.
class ButtonLongPressAction : public Action
{
public:
    using Action::Action;

    static const XrActionType ActionType = XR_ACTION_TYPE_BOOLEAN_INPUT;

    void update() override
    {
        mActive = false;
        bool old = mPressed;
        mXRAction.getBool(0, mPressed);
        bool changed = old != mPressed;
        if (changed && mPressed)
        {
            mPressTime = std::chrono::steady_clock::now();
            mTimein = mPressTime + gActionTime;
            mActivated = false;
        }
        if (mPressed && !mActivated)
        {
            if (std::chrono::steady_clock::now() >= mTimein)
            {
                mActive = mActivated = true;
                mValue = 1.f;
            }
        }
        if (changed && !mPressed)
        {
            mValue = 0.f;
        }
    }

    virtual bool shouldQueue() const override { return onActivate() || onDeactivate(); }

    bool mPressed{ false };
    bool mActivated{ false };
    std::chrono::steady_clock::time_point mPressTime{};
    std::chrono::steady_clock::time_point mTimein{};
};

//! Action that is active whenever the button is pressed down.
//! Useful for e.g. non-toggling sneak and automatically repeating actions
class ButtonHoldAction : public Action
{
public:
    using Action::Action;

    static const XrActionType ActionType = XR_ACTION_TYPE_BOOLEAN_INPUT;

    void update() override
    {
        mXRAction.getBool(0, mPressed);
        mActive = mPressed;

        mValue = mPressed ? 1.f : 0.f;
    }

    virtual bool shouldQueue() const override { return mActive || onDeactivate(); }

    bool mPressed{ false };
};

//! Action for axis actions, such as thumbstick axes or certain triggers/squeeze levers.
//! Float axis are considered active whenever their magnitude is greater than gAxisEpsilon. This is useful
//! as a touch subtitute on levers without touch.
class AxisAction : public Action
{
public:
    using Action::Action;

    static const XrActionType ActionType = XR_ACTION_TYPE_FLOAT_INPUT;

    void update() override
    {
        mActive = false;
        mXRAction.getFloat(0, mValue);

        if (std::fabs(mValue) > gAxisEpsilon)
            mActive = true;
        else
            mValue = 0.f;
    }

    virtual bool shouldQueue() const override { return mActive || onDeactivate(); }
};

struct OpenXRInput
{
    using Actions = MWInput::Actions;

    // The OpenMW input manager iterates from 0 to A_Last in its actions enum.
    // I don't know that it would cause any ill effects, but i nonetheless do not
    // want to contaminate the input manager with my OpenXR specific actions.
    // Therefore i add them here to a separate enum whose values start past A_Last.
    enum XrActions
    {
        A_XrFirst = MWInput::A_Last,
        A_ActivateTouch,
        A_RepositionMenu,
        A_XrLast
    };

    enum SubAction : signed
    {
        NONE = -1, //!< Used to ignore subaction or when action has no subaction. Not a valid input to createMWAction().
        // hands should be 0 and 1 as they are the typical use case of needing to index between 2 actions or subactions.
        LEFT_HAND = 0, //!< Read action from left-hand controller
        RIGHT_HAND = 1, //!< Read action from right-hand controller
        GAMEPAD = 2, //!< Read action from a gamepad
        HEAD = 3, //!< Read action from HMD
        USER = 4, //!< Read action from other devices

        SUBACTION_MAX = USER, //!< Used to size subaction arrays. Not a valid input.
    };

    template<size_t Size>
    using ActionPaths = std::array<XrPath, Size>;
    using SubActionPaths = ActionPaths<SUBACTION_MAX + 1>;
    using ControllerActionPaths = ActionPaths<3>; // left hand, right hand, and gamepad

    XrPath generateXrPath(const std::string& path);

    ControllerActionPaths generateControllerActionPaths(const std::string& controllerAction);

    OpenXRInput();

    template<typename A, XrActionType AT = A::ActionType>
    ActionPtr createMWAction(int openMWAction, const std::string& actionName, const std::string& localName, const std::vector<SubAction>& subActions = {});
    OpenXRAction createXRAction(XrActionType actionType, const std::string& actionName, const std::string& localName, const std::vector<SubAction>& subActions = {});

    XrActionSet createActionSet(void);

    XrPath subactionPath(SubAction subAction);

    void updateControls();
    const Action* nextAction();
    Pose getHandPose(int64_t time, TrackedSpace space, Side side);

    void applyHaptics(SubAction subAction, float intensity)
    {
        mHapticsAction.applyHaptics(subactionPath(subAction), intensity);
    }

    SubActionPaths mSubactionPath;
    XrActionSet mActionSet = XR_NULL_HANDLE;

    ControllerActionPaths mSelectPath;
    ControllerActionPaths mSqueezeValuePath;
    ControllerActionPaths mPosePath;
    ControllerActionPaths mHapticPath;
    ControllerActionPaths mMenuClickPath;
    ControllerActionPaths mThumbstickPath;
    ControllerActionPaths mThumbstickXPath;
    ControllerActionPaths mThumbstickYPath;
    ControllerActionPaths mThumbstickClickPath;
    ControllerActionPaths mXPath;
    ControllerActionPaths mYPath;
    ControllerActionPaths mAPath;
    ControllerActionPaths mBPath;
    ControllerActionPaths mTriggerValuePath;

    ActionPtr mGameMenu;
    ActionPtr mRepositionMenu;
    ActionPtr mInventory;
    ActionPtr mActivate;
    ActionPtr mUse;
    ActionPtr mJump;
    ActionPtr mToggleWeapon;
    ActionPtr mToggleSpell;
    ActionPtr mCycleSpellLeft;
    ActionPtr mCycleSpellRight;
    ActionPtr mCycleWeaponLeft;
    ActionPtr mCycleWeaponRight;
    ActionPtr mSneak;
    ActionPtr mQuickMenu;
    ActionPtr mLookLeftRight;
    ActionPtr mMoveForwardBackward;
    ActionPtr mMoveLeftRight;
    ActionPtr mJournal;
    ActionPtr mQuickSave;
    ActionPtr mRest;
    ActionPtr mActivateTouch;
    ActionPtr mAlwaysRun;
    ActionPtr mAutoMove;
    ActionPtr mToggleHUD;
    ActionPtr mToggleDebug;

    // Hand tracking
    OpenXRAction mHandPoseAction;
    OpenXRAction mHapticsAction;
    std::array<XrSpace, 2> mHandSpace;
    std::array<float, 2> mHandScale;
    std::array<XrBool32, 2> mHandActive;
    std::array<XrSpaceLocation, 2> mHandSpaceLocation{ { {XR_TYPE_SPACE_LOCATION}, {XR_TYPE_SPACE_LOCATION } } };

    // Head tracking
    osg::Vec3f mHeadForward;
    osg::Vec3f mHeadUpward;
    osg::Vec3f mHeadRightward;

    osg::Vec3f  mHeadStagePosition;
    osg::Quat   mHeadStageOrientation;
    osg::Vec3f  mHeadWorldPosition;
    osg::Quat   mHeadWorldOrientation;


    std::deque<const Action*> mActionQueue{};
};

XrActionSet
OpenXRInput::createActionSet()
{
    auto* xr = Environment::get().getManager();
    XrActionSet actionSet = XR_NULL_HANDLE;
    XrActionSetCreateInfo createInfo{ XR_TYPE_ACTION_SET_CREATE_INFO };
    strcpy_s(createInfo.actionSetName, "gameplay");
    strcpy_s(createInfo.localizedActionSetName, "Gameplay");
    createInfo.priority = 0;
    CHECK_XRCMD(xrCreateActionSet(xr->impl().mInstance, &createInfo, &actionSet));
    return actionSet;
}

void
OpenXRAction::operator=(
    OpenXRAction&& rhs)
{
    if (mAction)
        xrDestroyAction(mAction);
    *this = static_cast<OpenXRAction&>(rhs);
    rhs.mAction = XR_NULL_HANDLE;
}


OpenXRAction::OpenXRAction(
    XrAction action,
    XrActionType actionType,
    const std::string& actionName,
    const std::string& localName)
    : mAction(action)
    , mType(actionType)
    , mName(actionName)
    , mLocalName(localName)
{
};

OpenXRAction::~OpenXRAction() {
    if (mAction)
    {
        xrDestroyAction(mAction);
        std::cout << "I destroyed your \"" << mLocalName << "\" action" << std::endl;
    }
}

bool OpenXRAction::getFloat(XrPath subactionPath, float& value)
{
    auto* xr = Environment::get().getManager();
    XrActionStateGetInfo getInfo{ XR_TYPE_ACTION_STATE_GET_INFO };
    getInfo.action = mAction;
    getInfo.subactionPath = subactionPath;

    XrActionStateFloat xrValue{ XR_TYPE_ACTION_STATE_FLOAT };
    CHECK_XRCMD(xrGetActionStateFloat(xr->impl().mSession, &getInfo, &xrValue));

    if (xrValue.isActive)
        value = xrValue.currentState;
    return xrValue.isActive;
}

bool OpenXRAction::getBool(XrPath subactionPath, bool& value)
{
    auto* xr = Environment::get().getManager();
    XrActionStateGetInfo getInfo{ XR_TYPE_ACTION_STATE_GET_INFO };
    getInfo.action = mAction;
    getInfo.subactionPath = subactionPath;

    XrActionStateBoolean xrValue{ XR_TYPE_ACTION_STATE_BOOLEAN };
    CHECK_XRCMD(xrGetActionStateBoolean(xr->impl().mSession, &getInfo, &xrValue));

    if (xrValue.isActive)
        value = xrValue.currentState;
    return xrValue.isActive;
}

// Pose action only checks if the pose is active or not
bool OpenXRAction::getPose(XrPath subactionPath)
{
    auto* xr = Environment::get().getManager();
    XrActionStateGetInfo getInfo{ XR_TYPE_ACTION_STATE_GET_INFO };
    getInfo.action = mAction;
    getInfo.subactionPath = subactionPath;

    XrActionStatePose xrValue{ XR_TYPE_ACTION_STATE_POSE };
    CHECK_XRCMD(xrGetActionStatePose(xr->impl().mSession, &getInfo, &xrValue));

    return xrValue.isActive;
}

bool OpenXRAction::applyHaptics(XrPath subactionPath, float amplitude)
{
    amplitude = std::max(0.f, std::min(1.f, amplitude));

    auto* xr = Environment::get().getManager();
    XrHapticVibration vibration{ XR_TYPE_HAPTIC_VIBRATION };
    vibration.amplitude = amplitude;
    vibration.duration = XR_MIN_HAPTIC_DURATION;
    vibration.frequency = XR_FREQUENCY_UNSPECIFIED;

    XrHapticActionInfo hapticActionInfo{ XR_TYPE_HAPTIC_ACTION_INFO };
    hapticActionInfo.action = mAction;
    hapticActionInfo.subactionPath = subactionPath;
    CHECK_XRCMD(xrApplyHapticFeedback(xr->impl().mSession, &hapticActionInfo, (XrHapticBaseHeader*)&vibration));
    return true;
}

OpenXRInput::ControllerActionPaths
OpenXRInput::generateControllerActionPaths(
    const std::string& controllerAction)
{
    auto* xr = Environment::get().getManager();
    ControllerActionPaths actionPaths;

    std::string left = std::string("/user/hand/left") + controllerAction;
    std::string right = std::string("/user/hand/right") + controllerAction;
    std::string pad = std::string("/user/gamepad") + controllerAction;

    CHECK_XRCMD(xrStringToPath(xr->impl().mInstance, left.c_str(), &actionPaths[LEFT_HAND]));
    CHECK_XRCMD(xrStringToPath(xr->impl().mInstance, right.c_str(), &actionPaths[RIGHT_HAND]));
    CHECK_XRCMD(xrStringToPath(xr->impl().mInstance, pad.c_str(), &actionPaths[GAMEPAD]));

    return actionPaths;
}

OpenXRInput::OpenXRInput()
    : mSubactionPath{ {
            generateXrPath("/user/hand/left"),
            generateXrPath("/user/hand/right"),
            generateXrPath("/user/gamepad"),
            generateXrPath("/user/head"),
            generateXrPath("/user")
        } }
    , mActionSet(createActionSet())
    , mSelectPath(generateControllerActionPaths("/input/select/click"))
    , mSqueezeValuePath(generateControllerActionPaths("/input/squeeze/value"))
    , mPosePath(generateControllerActionPaths("/input/aim/pose"))
    , mHapticPath(generateControllerActionPaths("/output/haptic"))
    , mMenuClickPath(generateControllerActionPaths("/input/menu/click"))
    , mThumbstickPath(generateControllerActionPaths("/input/thumbstick/value"))
    , mThumbstickXPath(generateControllerActionPaths("/input/thumbstick/x"))
    , mThumbstickYPath(generateControllerActionPaths("/input/thumbstick/y"))
    , mThumbstickClickPath(generateControllerActionPaths("/input/thumbstick/click"))
    , mXPath(generateControllerActionPaths("/input/x/click"))
    , mYPath(generateControllerActionPaths("/input/y/click"))
    , mAPath(generateControllerActionPaths("/input/a/click"))
    , mBPath(generateControllerActionPaths("/input/b/click"))
    , mTriggerValuePath(generateControllerActionPaths("/input/trigger/value"))
    , mGameMenu(std::move(createMWAction<ButtonPressAction>(MWInput::A_GameMenu, "game_menu", "Game Menu", { })))
    , mRepositionMenu(std::move(createMWAction<ButtonLongPressAction>(A_RepositionMenu, "reposition_menu", "Reposition Menu", { })))
    , mInventory(std::move(createMWAction<ButtonPressAction>(MWInput::A_Inventory, "inventory", "Inventory", { })))
    , mActivate(std::move(createMWAction<ButtonPressAction>(MWInput::A_Activate, "activate", "Activate", { })))
    , mUse(std::move(createMWAction<ButtonHoldAction>(MWInput::A_Use, "use", "Use", { })))
    , mJump(std::move(createMWAction<ButtonPressAction>(MWInput::A_Jump, "jump", "Jump", { })))
    , mToggleWeapon(std::move(createMWAction<ButtonPressAction>(MWInput::A_ToggleWeapon, "weapon", "Weapon", { })))
    , mToggleSpell(std::move(createMWAction<ButtonPressAction>(MWInput::A_ToggleSpell, "spell", "Spell", { })))
    , mCycleSpellLeft(std::move(createMWAction<ButtonPressAction>(MWInput::A_CycleSpellLeft, "cycle_spell_left", "Cycle Spell Left", { })))
    , mCycleSpellRight(std::move(createMWAction<ButtonPressAction>(MWInput::A_CycleSpellRight, "cycle_spell_right", "Cycle Spell Right", { })))
    , mCycleWeaponLeft(std::move(createMWAction<ButtonPressAction>(MWInput::A_CycleWeaponLeft, "cycle_weapon_left", "Cycle Weapon Left", { })))
    , mCycleWeaponRight(std::move(createMWAction<ButtonPressAction>(MWInput::A_CycleWeaponRight, "cycle_weapon_right", "Cycle Weapon Right", { })))
    , mSneak(std::move(createMWAction<ButtonHoldAction>(MWInput::A_Sneak, "sneak", "Sneak", { })))
    , mQuickMenu(std::move(createMWAction<ButtonPressAction>(MWInput::A_QuickMenu, "quick_menu", "Quick Menu", { })))
    , mLookLeftRight(std::move(createMWAction<AxisAction>(MWInput::A_LookLeftRight, "look_left_right", "Look Left Right", { })))
    , mMoveForwardBackward(std::move(createMWAction<AxisAction>(MWInput::A_MoveForwardBackward, "move_forward_backward", "Move Forward Backward", { })))
    , mMoveLeftRight(std::move(createMWAction<AxisAction>(MWInput::A_MoveLeftRight, "move_left_right", "Move Left Right", { })))
    , mJournal(std::move(createMWAction<ButtonLongPressAction>(MWInput::A_Journal, "journal_book", "Journal Book", { })))
    , mQuickSave(std::move(createMWAction<ButtonLongPressAction>(MWInput::A_QuickSave, "quick_save", "Quick Save", { })))
    , mRest(std::move(createMWAction<ButtonLongPressAction>(MWInput::A_Rest, "rest", "Rest", { })))
    , mActivateTouch(std::move(createMWAction<AxisAction>(A_ActivateTouch, "activate_touched", "Activate Touch", { RIGHT_HAND })))
    , mAlwaysRun(std::move(createMWAction<ButtonPressAction>(MWInput::A_AlwaysRun, "always_run", "Always Run", { })))
    , mAutoMove(std::move(createMWAction<ButtonPressAction>(MWInput::A_AutoMove, "auto_move", "Auto Move", { })))
    , mToggleHUD(std::move(createMWAction<ButtonLongPressAction>(MWInput::A_ToggleHUD, "toggle_hud", "Toggle HUD", { })))
    , mToggleDebug(std::move(createMWAction<ButtonLongPressAction>(MWInput::A_ToggleDebug, "toggle_debug", "Toggle DEBUG", { })))
    , mHandPoseAction(std::move(createXRAction(XR_ACTION_TYPE_POSE_INPUT, "hand_pose", "Hand Pose", { LEFT_HAND, RIGHT_HAND })))
    , mHapticsAction(std::move(createXRAction(XR_ACTION_TYPE_VIBRATION_OUTPUT, "vibrate_hand", "Vibrate Hand", { LEFT_HAND, RIGHT_HAND })))
{
    auto* xr = Environment::get().getManager();
    { // Set up default bindings for the oculus
        XrPath oculusTouchInteractionProfilePath;
        CHECK_XRCMD(
            xrStringToPath(xr->impl().mInstance, "/interaction_profiles/oculus/touch_controller", &oculusTouchInteractionProfilePath));

        /*
            // Applicable actions not (yet) included
            A_QuickKey1,
            A_QuickKey2,
            A_QuickKey3,
            A_QuickKey4,
            A_QuickKey5,
            A_QuickKey6,
            A_QuickKey7,
            A_QuickKey8,
            A_QuickKey9,
            A_QuickKey10,
            A_QuickKeysMenu,
            A_QuickLoad,
            A_CycleSpellLeft,           
            A_CycleSpellRight,
            A_CycleWeaponLeft,          
            A_CycleWeaponRight,
            A_Screenshot, // Generate a VR screenshot?
            A_Console,    // Currently awkward due to a lack of virtual keyboard, but should be included when that's in place
        */

        /*
            Oculus Bindings:
            L-Squeeze:
                Hold: Sneak

            R-Squeeze:
                Hold: Enable Pointer

            L-Trigger:
                Press: Jump

            R-Trigger:
                IF POINTER: 
                    Activate
                ELSE:
                    Use

            L-Thumbstick:
                X-Axis: MoveForwardBackward
                Y-Axis: MoveLeftRight
                Button: 
                    Press: AlwaysRun
                    Long: ToggleHUD
                Touch: 

            R-Thumbstick:
                X-Axis: LookLeftRight
                Y-Axis: 
                Button: 
                    Press: AutoMove
                    Long: ToggleDebug
                Touch: 

            X:
                Press: Toggle Spell
                Long: 
            Y:
                Press: Rest
                Long: Quick Save
            A:
                Press: Toggle Weapon
                Long: 
            B:
                Press: Inventory
                Long: Journal

            Menu:
                Press: GameMenun
                Long: Reposition GUI

        */

        std::vector<XrActionSuggestedBinding> bindings{ {
            {mHandPoseAction, mPosePath[LEFT_HAND]},
            {mHandPoseAction, mPosePath[RIGHT_HAND]},
            {mHapticsAction, mHapticPath[LEFT_HAND]},
            {mHapticsAction, mHapticPath[RIGHT_HAND]},
            {*mLookLeftRight, mThumbstickXPath[RIGHT_HAND]},
            {*mMoveLeftRight, mThumbstickXPath[LEFT_HAND]},
            {*mMoveForwardBackward, mThumbstickYPath[LEFT_HAND]},
            {*mActivate, mSqueezeValuePath[RIGHT_HAND]},
            {*mUse, mTriggerValuePath[RIGHT_HAND]},
            {*mJump, mTriggerValuePath[LEFT_HAND]},
            {*mToggleWeapon, mAPath[RIGHT_HAND]},
            {*mToggleSpell, mXPath[LEFT_HAND]},
            //{*mCycleSpellLeft, mThumbstickClickPath[LEFT_HAND]},
            //{*mCycleSpellRight, mThumbstickClickPath[RIGHT_HAND]},
            //{*mCycleWeaponLeft, mThumbstickClickPath[LEFT_HAND]},
            //{*mCycleWeaponRight, mThumbstickClickPath[RIGHT_HAND]},
            {*mAlwaysRun, mThumbstickClickPath[LEFT_HAND]},
            {*mAutoMove, mThumbstickClickPath[RIGHT_HAND]},
            {*mToggleHUD, mThumbstickClickPath[LEFT_HAND]},
            {*mToggleDebug, mThumbstickClickPath[RIGHT_HAND]},
            {*mSneak, mSqueezeValuePath[LEFT_HAND]},
            {*mInventory, mBPath[RIGHT_HAND]},
            {*mRest, mYPath[LEFT_HAND]},
            {*mJournal, mBPath[RIGHT_HAND]},
            {*mQuickSave, mYPath[LEFT_HAND]},
            {*mGameMenu, mMenuClickPath[LEFT_HAND]},
            {*mRepositionMenu, mMenuClickPath[LEFT_HAND]},
            {*mActivateTouch, mSqueezeValuePath[RIGHT_HAND]},
          } };
        XrInteractionProfileSuggestedBinding suggestedBindings{ XR_TYPE_INTERACTION_PROFILE_SUGGESTED_BINDING };
        suggestedBindings.interactionProfile = oculusTouchInteractionProfilePath;
        suggestedBindings.suggestedBindings = bindings.data();
        suggestedBindings.countSuggestedBindings = (uint32_t)bindings.size();
        CHECK_XRCMD(xrSuggestInteractionProfileBindings(xr->impl().mInstance, &suggestedBindings));
    }

    { // Set up action spaces
        XrActionSpaceCreateInfo createInfo{ XR_TYPE_ACTION_SPACE_CREATE_INFO };
        createInfo.action = mHandPoseAction;
        createInfo.poseInActionSpace.orientation.w = 1.f;
        createInfo.subactionPath = mSubactionPath[LEFT_HAND];
        CHECK_XRCMD(xrCreateActionSpace(xr->impl().mSession, &createInfo, &mHandSpace[LEFT_HAND]));
        createInfo.subactionPath = mSubactionPath[RIGHT_HAND];
        CHECK_XRCMD(xrCreateActionSpace(xr->impl().mSession, &createInfo, &mHandSpace[RIGHT_HAND]));
    }

    { // Set up the action set
        XrSessionActionSetsAttachInfo attachInfo{ XR_TYPE_SESSION_ACTION_SETS_ATTACH_INFO };
        attachInfo.countActionSets = 1;
        attachInfo.actionSets = &mActionSet;
        CHECK_XRCMD(xrAttachSessionActionSets(xr->impl().mSession, &attachInfo));
    }
};

OpenXRAction 
OpenXRInput::createXRAction(
    XrActionType actionType, 
    const std::string& actionName, 
    const std::string& localName, 
    const std::vector<SubAction>& subActions)
{
    ActionPtr actionPtr = nullptr;
    std::vector<XrPath> subactionPaths;
    XrActionCreateInfo createInfo{ XR_TYPE_ACTION_CREATE_INFO };
    createInfo.actionType = actionType;
    strcpy_s(createInfo.actionName, actionName.c_str());
    strcpy_s(createInfo.localizedActionName, localName.c_str());

    if (!subActions.empty())
    {
        for (auto subAction : subActions)
            subactionPaths.push_back(subactionPath(subAction));
        createInfo.countSubactionPaths = subactionPaths.size();
        createInfo.subactionPaths = subactionPaths.data();
    }

    XrAction action = XR_NULL_HANDLE;
    CHECK_XRCMD(xrCreateAction(mActionSet, &createInfo, &action));
    return OpenXRAction(action, actionType, actionName, localName);
}

template<typename A, XrActionType AT>
ActionPtr
OpenXRInput::createMWAction(
    int openMWAction,
    const std::string& actionName,
    const std::string& localName,
    const std::vector<SubAction>& subActions)
{
    return ActionPtr(new A(openMWAction, std::move(createXRAction(AT, actionName, localName, subActions))));
}

XrPath
OpenXRInput::subactionPath(
    SubAction subAction)
{
    if (subAction == NONE)
        return 0;
    return mSubactionPath[subAction];
}

void
OpenXRInput::updateControls()
{
    auto* xr = Environment::get().getManager();
    if (!xr->impl().mSessionRunning)
        return;


    const XrActiveActionSet activeActionSet{ mActionSet, XR_NULL_PATH };
    XrActionsSyncInfo syncInfo{ XR_TYPE_ACTIONS_SYNC_INFO };
    syncInfo.countActiveActionSets = 1;
    syncInfo.activeActionSets = &activeActionSet;
    CHECK_XRCMD(xrSyncActions(xr->impl().mSession, &syncInfo));

    mGameMenu->updateAndQueue(mActionQueue);
    mRepositionMenu->updateAndQueue(mActionQueue);
    mInventory->updateAndQueue(mActionQueue);
    mActivate->updateAndQueue(mActionQueue);
    mUse->updateAndQueue(mActionQueue);
    mJump->updateAndQueue(mActionQueue);
    mToggleWeapon->updateAndQueue(mActionQueue);
    mToggleSpell->updateAndQueue(mActionQueue);
    mCycleSpellLeft->updateAndQueue(mActionQueue);
    mCycleSpellRight->updateAndQueue(mActionQueue);
    mCycleWeaponLeft->updateAndQueue(mActionQueue);
    mCycleWeaponRight->updateAndQueue(mActionQueue);
    mSneak->updateAndQueue(mActionQueue);
    mQuickMenu->updateAndQueue(mActionQueue);
    mLookLeftRight->updateAndQueue(mActionQueue);
    mMoveForwardBackward->updateAndQueue(mActionQueue);
    mMoveLeftRight->updateAndQueue(mActionQueue);
    mJournal->updateAndQueue(mActionQueue);
    mQuickSave->updateAndQueue(mActionQueue);
    mRest->updateAndQueue(mActionQueue);
    mActivateTouch->updateAndQueue(mActionQueue);
    mAlwaysRun->updateAndQueue(mActionQueue);
    mAutoMove->updateAndQueue(mActionQueue);
    mToggleHUD->updateAndQueue(mActionQueue);
    mToggleDebug->updateAndQueue(mActionQueue);
}

XrPath OpenXRInput::generateXrPath(const std::string& path)
{
    auto* xr = Environment::get().getManager();
    XrPath xrpath = 0;
    CHECK_XRCMD(xrStringToPath(xr->impl().mInstance, path.c_str(), &xrpath));
    return xrpath;
}

const Action* OpenXRInput::nextAction()
{
    if (mActionQueue.empty())
        return nullptr;

    const auto* action = mActionQueue.front();
    mActionQueue.pop_front();
    return action;

}

Pose
OpenXRInput::getHandPose(
    int64_t time,
    TrackedSpace space,
    Side side)
{
    auto* xr = Environment::get().getManager();
    XrSpace referenceSpace = xr->impl().getReferenceSpace(space);

    XrSpaceLocation location{ XR_TYPE_SPACE_LOCATION };
    XrSpaceVelocity velocity{ XR_TYPE_SPACE_VELOCITY };
    location.next = &velocity;
    CHECK_XRCMD(xrLocateSpace(mHandSpace[(int)side], referenceSpace, time, &location));
    if (!location.locationFlags & XR_SPACE_LOCATION_ORIENTATION_VALID_BIT)
        // Quat must have a magnitude of 1 but openxr sets it to 0 when tracking is unavailable.
        // I want a no-track pose to still be a valid quat so osg won't throw errors
        location.pose.orientation.w = 1;

    return MWVR::Pose{
        osg::fromXR(location.pose.position),
        osg::fromXR(location.pose.orientation),
        osg::fromXR(velocity.linearVelocity)
    };
}


Pose OpenXRInputManager::getHandPose(int64_t time, TrackedSpace space, Side side)
{
    return mXRInput->getHandPose(time, space, side);
}

void OpenXRInputManager::updateActivationIndication(void)
{
    bool guiMode = MWBase::Environment::get().getWindowManager()->isGuiMode();
    bool show = guiMode | mActivationIndication;
    auto* playerAnimation = Environment::get().getPlayerAnimation();
    if (playerAnimation)
        if (show != playerAnimation->isPointingForward())
            playerAnimation->setPointForward(show);
}


/**
 * Makes it possible to use ItemModel::moveItem to move an item from an inventory to the world.
 */
class DropItemAtPointModel: public MWGui::ItemModel
{
public:
    DropItemAtPointModel(){}
    virtual ~DropItemAtPointModel() {}
    virtual MWWorld::Ptr copyItem(const MWGui::ItemStack& item, size_t count, bool /*allowAutoEquip*/)
    {
        MWBase::World* world = MWBase::Environment::get().getWorld();
        MWVR::VRAnimation* anim = MWVR::Environment::get().getPlayerAnimation();

        MWWorld::Ptr dropped;
        if (anim->canPlaceObject())
            dropped = world->placeObject(item.mBase, anim->getPointerTarget(), count);
        else
            dropped = world->dropObjectOnGround(world->getPlayerPtr(), item.mBase, count);
        dropped.getCellRef().setOwner("");

        return dropped;
    }

    virtual void removeItem(const MWGui::ItemStack& item, size_t count) { throw std::runtime_error("removeItem not implemented"); }
    virtual ModelIndex getIndex(MWGui::ItemStack item) { throw std::runtime_error("getIndex not implemented"); }
    virtual void update() {}
    virtual size_t getItemCount() { return 0; }
    virtual MWGui::ItemStack getItem(ModelIndex index) { throw std::runtime_error("getItem not implemented"); }

private:
    // Where to drop the item
    MWRender::RayResult mIntersection;
};

    void OpenXRInputManager::pointActivation(bool onPress)
    {
        auto* world = MWBase::Environment::get().getWorld();
        auto* anim = MWVR::Environment::get().getPlayerAnimation();
        if (world && anim && anim->mPointerTarget.mHit)
        {
            auto* node = anim->mPointerTarget.mHitNode;
            MWWorld::Ptr ptr = anim->mPointerTarget.mHitObject;
            auto& dnd = MWBase::Environment::get().getWindowManager()->getDragAndDrop();

            if (node && node->getName() == "VRGUILayer")
            {
                injectMousePress(SDL_BUTTON_LEFT, onPress);
            }
            else if (onPress)
            {
                // Other actions should only happen on release;
                return;
            }
            else if (dnd.mIsOnDragAndDrop)
            {
                // Intersected with the world while drag and drop is active
                // Drop item into the world
                MWBase::Environment::get().getWorld()->breakInvisibility(
                    MWMechanics::getPlayer());
                DropItemAtPointModel drop;
                dnd.drop(&drop, nullptr);
            }
            else if (!ptr.isEmpty())
            {
                MWWorld::Player& player = MWBase::Environment::get().getWorld()->getPlayer();
                player.activate(ptr);
            }
        }
    }

    void OpenXRInputManager::injectMousePress(int sdlButton, bool onPress)
    {
        SDL_MouseButtonEvent arg;
        if (onPress)
            mMouseManager->mousePressed(arg, sdlButton);
        else
            mMouseManager->mouseReleased(arg, sdlButton);
    }

    void OpenXRInputManager::injectChannelValue(
        MWInput::Actions action, 
        float value)
    {
        auto channel = mBindingsManager->ics().getChannel(MWInput::A_MoveLeftRight);// ->setValue(value);
        channel->setEnabled(true);
    }

    // TODO: Configurable haptics: on/off + max intensity
    void OpenXRInputManager::applyHapticsLeftHand(float intensity)
    {
        mXRInput->applyHaptics(OpenXRInput::LEFT_HAND, intensity);
    }

    void OpenXRInputManager::applyHapticsRightHand(float intensity)
    {
        mXRInput->applyHaptics(OpenXRInput::RIGHT_HAND, intensity);
    }

    OpenXRInputManager::OpenXRInputManager(
        SDL_Window* window,
        osg::ref_ptr<osgViewer::Viewer> viewer, 
        osg::ref_ptr<osgViewer::ScreenCaptureHandler> screenCaptureHandler, 
        osgViewer::ScreenCaptureHandler::CaptureOperation* screenCaptureOperation, 
        const std::string& userFile, 
        bool userFileExists, 
        const std::string& userControllerBindingsFile, 
        const std::string& controllerBindingsFile, 
        bool grab)
        : MWInput::InputManager(
            window, 
            viewer, 
            screenCaptureHandler,
            screenCaptureOperation,
            userFile,
            userFileExists,
            userControllerBindingsFile,
            controllerBindingsFile,
            grab)
        , mXRInput(new OpenXRInput())
    {
        // VR mode should never go vanity mode.
        //mControlSwitch->set("vanitymode", false);
    }

    OpenXRInputManager::~OpenXRInputManager()
    {
    }

    void OpenXRInputManager::changeInputMode(bool mode)
    {
        // VR mode has no concept of these
        //mGuiCursorEnabled = false;
        MWInput::InputManager::changeInputMode(mode);
        MWBase::Environment::get().getWindowManager()->showCrosshair(false);
        MWBase::Environment::get().getWindowManager()->setCursorVisible(false);
    }

    void OpenXRInputManager::update(
        float dt,
        bool disableControls,
        bool disableEvents)
    {
        auto begin = std::chrono::steady_clock::now();
        mXRInput->updateControls();


        auto* vrGuiManager = Environment::get().getGUIManager();
        bool vrHasFocus = vrGuiManager->updateFocus();
        auto guiCursor = vrGuiManager->guiCursor();
        if (vrHasFocus)
        {
            mMouseManager->setMousePosition(guiCursor.x(), guiCursor.y());
        }

        while (auto* action = mXRInput->nextAction())
        {
            processAction(action);
        }

        updateActivationIndication();

        MWInput::InputManager::update(dt, disableControls, disableEvents);

        // The rest of this code assumes the game is running
        if (MWBase::Environment::get().getStateManager()->getState() != MWBase::StateManager::State_Running)
            return;

        bool guiMode = MWBase::Environment::get().getWindowManager()->isGuiMode();

        // OpenMW assumes all input will come via SDL which i often violate.
        // This keeps player controls correctly enabled for my purposes.
        mBindingsManager->setPlayerControlsEnabled(!guiMode);

        updateHead();

        if (!guiMode)
        {
            auto* world = MWBase::Environment::get().getWorld();

            auto& player = world->getPlayer();
            auto playerPtr = world->getPlayerPtr();
            if (!mRealisticCombat || mRealisticCombat->ptr != playerPtr)
                mRealisticCombat.reset(new RealisticCombat::StateMachine(playerPtr));
            bool enabled = !guiMode && player.getDrawState() == MWMechanics::DrawState_Weapon && !player.isDisabled();
            mRealisticCombat->update(dt, enabled);
        }
        else if(mRealisticCombat)
            mRealisticCombat->update(dt, false);


        // Update tracking every frame if player is not currently in GUI mode.
        // This ensures certain widgets like Notifications will be visible.
        if (!guiMode)
        {
            vrGuiManager->updateTracking();
        }
        auto end = std::chrono::steady_clock::now();

        auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(end - begin);
    }

    void OpenXRInputManager::processAction(const Action* action)
    {
        static const bool isToggleSneak = Settings::Manager::getBool("toggle sneak", "Input");
        auto* xrGUIManager = Environment::get().getGUIManager();

        // Hold actions
        switch (action->openMWActionCode())
        {
        case OpenXRInput::A_ActivateTouch:
            resetIdleTime();
            mActivationIndication = action->isActive();
            break;
        case MWInput::A_LookLeftRight:
            mYaw += osg::DegreesToRadians(action->value()) * 2.f;
            break;
        case MWInput::A_MoveLeftRight:
            mBindingsManager->ics().getChannel(MWInput::A_MoveLeftRight)->setValue(action->value() / 2.f + 0.5f);
            break;
        case MWInput::A_MoveForwardBackward:
            mBindingsManager->ics().getChannel(MWInput::A_MoveForwardBackward)->setValue(-action->value() / 2.f + 0.5f);
            break;
        case MWInput::A_Sneak:
        {
            if (!isToggleSneak)
                mBindingsManager->ics().getChannel(MWInput::A_Sneak)->setValue(action->isActive() ? 1.f : 0.f);
            break;
        }
        case MWInput::A_Use:
            if (!(mActivationIndication || MWBase::Environment::get().getWindowManager()->isGuiMode()))
                mBindingsManager->ics().getChannel(MWInput::A_Use)->setValue(action->value());
            break;
        default:
            break;
        }

        // OnActivate actions
        if (action->onActivate())
        {
            switch (action->openMWActionCode())
            {
            case MWInput::A_GameMenu:
                mActionManager->toggleMainMenu();
                break;
            case MWInput::A_Screenshot:
                mActionManager->screenshot();
                break;
            case MWInput::A_Inventory:
                //mActionManager->toggleInventory();
                injectMousePress(SDL_BUTTON_RIGHT, true);
                break;
            case MWInput::A_Console:
                mActionManager->toggleConsole();
                break;
            case MWInput::A_Journal:
                mActionManager->toggleJournal();
                break;
            case MWInput::A_AutoMove:
                mActionManager->toggleAutoMove();
                break;
            case MWInput::A_AlwaysRun:
                mActionManager->toggleWalking();
                break;
            case MWInput::A_ToggleWeapon:
                mActionManager->toggleWeapon();
                break;
            case MWInput::A_Rest:
                mActionManager->rest();
                break;
            case MWInput::A_ToggleSpell:
                mActionManager->toggleSpell();
                break;
            case MWInput::A_QuickKey1:
                mActionManager->quickKey(1);
                break;
            case MWInput::A_QuickKey2:
                mActionManager->quickKey(2);
                break;
            case MWInput::A_QuickKey3:
                mActionManager->quickKey(3);
                break;
            case MWInput::A_QuickKey4:
                mActionManager->quickKey(4);
                break;
            case MWInput::A_QuickKey5:
                mActionManager->quickKey(5);
                break;
            case MWInput::A_QuickKey6:
                mActionManager->quickKey(6);
                break;
            case MWInput::A_QuickKey7:
                mActionManager->quickKey(7);
                break;
            case MWInput::A_QuickKey8:
                mActionManager->quickKey(8);
                break;
            case MWInput::A_QuickKey9:
                mActionManager->quickKey(9);
                break;
            case MWInput::A_QuickKey10:
                mActionManager->quickKey(10);
                break;
            case MWInput::A_QuickKeysMenu:
                mActionManager->showQuickKeysMenu();
                break;
            case MWInput::A_ToggleHUD:
                Log(Debug::Verbose) << "Toggle HUD";
                MWBase::Environment::get().getWindowManager()->toggleHud();
                break;
            case MWInput::A_ToggleDebug:
                Log(Debug::Verbose) << "Toggle Debug";
                MWBase::Environment::get().getWindowManager()->toggleDebugWindow();
                break;
            case MWInput::A_QuickSave:
                mActionManager->quickSave();
                break;
            case MWInput::A_QuickLoad:
                mActionManager->quickLoad();
                break;
            case MWInput::A_CycleSpellLeft:
                if (mActionManager->checkAllowedToUseItems() && MWBase::Environment::get().getWindowManager()->isAllowed(MWGui::GW_Magic))
                    MWBase::Environment::get().getWindowManager()->cycleSpell(false);
                break;
            case MWInput::A_CycleSpellRight:
                if (mActionManager->checkAllowedToUseItems() && MWBase::Environment::get().getWindowManager()->isAllowed(MWGui::GW_Magic))
                    MWBase::Environment::get().getWindowManager()->cycleSpell(true);
                break;
            case MWInput::A_CycleWeaponLeft:
                if (mActionManager->checkAllowedToUseItems() && MWBase::Environment::get().getWindowManager()->isAllowed(MWGui::GW_Inventory))
                    MWBase::Environment::get().getWindowManager()->cycleWeapon(false);
                break;
            case MWInput::A_CycleWeaponRight:
                if (mActionManager->checkAllowedToUseItems() && MWBase::Environment::get().getWindowManager()->isAllowed(MWGui::GW_Inventory))
                    MWBase::Environment::get().getWindowManager()->cycleWeapon(true);
                break;
            case MWInput::A_Jump:
                mActionManager->setAttemptJump(true);
                break;
            case OpenXRInput::A_RepositionMenu:
                xrGUIManager->updateTracking();
                break;
            case MWInput::A_Use:
                if (mActivationIndication || MWBase::Environment::get().getWindowManager()->isGuiMode())
                    pointActivation(true);
            default:
                break;
            }
        }

        // A few actions need to fire on deactivation
        if (action->onDeactivate())
        {
            switch (action->openMWActionCode())
            {
            case MWInput::A_Use:
                mBindingsManager->ics().getChannel(MWInput::A_Use)->setValue(0.f);
                if (mActivationIndication || MWBase::Environment::get().getWindowManager()->isGuiMode())
                    pointActivation(false);
                break;
            case MWInput::A_Sneak:
                if (isToggleSneak)
                    mActionManager->toggleSneaking();
                break;
            case MWInput::A_Inventory:
                injectMousePress(SDL_BUTTON_RIGHT, false);
            default:
                break;
            }
        }
    }

    void OpenXRInputManager::updateHead()
    {
        auto* session = Environment::get().getSession();
        auto currentHeadPose = session->predictedPoses(VRSession::FramePhase::Predraw).head;
        currentHeadPose.position *= Environment::get().unitsPerMeter();
        osg::Vec3 vrMovement = currentHeadPose.position - mHeadPose.position;
        mHeadPose = currentHeadPose;
        osg::Quat gameworldYaw = osg::Quat(mYaw, osg::Vec3(0, 0, -1));
        mHeadOffset += gameworldYaw * vrMovement;

        if (mRecenter)
        {
            mHeadOffset = osg::Vec3(0, 0, 0);
            // Z should not be affected
            mHeadOffset.z() = mHeadPose.position.z();
            mRecenter = false;
        }
        else
        {
            MWBase::World* world = MWBase::Environment::get().getWorld();
            auto& player = world->getPlayer();
            auto playerPtr = player.getPlayer();

            float yaw = 0.f;
            float pitch = 0.f;
            float roll = 0.f;
            getEulerAngles(mHeadPose.orientation, yaw, pitch, roll);

            yaw += mYaw;

            mVrAngles[0] = pitch;
            mVrAngles[1] = roll;
            mVrAngles[2] = yaw;

            if (!player.isDisabled())
            {
                world->rotateObject(playerPtr, mVrAngles[0], mVrAngles[1], mVrAngles[2], MWBase::RotationFlag_none);
            }
            else {
                // Update the camera directly to avoid rotating the disabled player
                world->getRenderingManager().getCamera()->rotateCamera(-pitch, -roll, -yaw, false);
            }
        }

    }
}
