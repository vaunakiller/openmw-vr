#include "openxrinputmanager.hpp"
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

#include "../mwworld/player.hpp"
#include "../mwworld/class.hpp"
#include "../mwworld/inventorystore.hpp"
#include "../mwworld/esmstore.hpp"

#include <openxr/openxr.h>

#include <osg/Camera>

#include <vector>
#include <deque>
#include <array>
#include <iostream>

namespace MWVR
{
    struct OpenXRActionEvent
    {
        MWInput::InputManager::Actions action;
        bool onPress;
        float value = 0.f;
    };

    struct OpenXRAction
    {

        // I want these to manage XrAction objects but i also didn't want to wrap these in unique pointers (useless dynamic allocation).
        // So i implemented move for them instead.
        OpenXRAction() = delete;
        OpenXRAction(OpenXRAction&& rhs) { mAction = nullptr; *this = std::move(rhs); }
        void operator=(OpenXRAction&& rhs);
    private:
        OpenXRAction(const OpenXRAction&) = default;
        OpenXRAction& operator=(const OpenXRAction&) = default;
    public:
        OpenXRAction(osg::ref_ptr<OpenXRManager> XR, XrAction action, XrActionType actionType, const std::string& actionName, const std::string& localName);
        ~OpenXRAction();


        operator XrAction() { return mAction; }

        bool getFloat(XrPath subactionPath, float& value);
        bool getBool(XrPath subactionPath, bool& value);
        bool getPose(XrPath subactionPath);
        bool applyHaptics(XrPath subactionPath, float amplitude);

        osg::ref_ptr<OpenXRManager> mXR = nullptr;
        XrAction mAction = XR_NULL_HANDLE;
        XrActionType mType;
        std::string mName;
        std::string mLocalName;
    };

    //! Wrapper around bool type input actions, ignoring all subactions
    struct OpenXRAction_Bool
    {
        OpenXRAction_Bool(OpenXRAction action);
        operator XrAction() { return mAction; }

        void update();

        //! True if action changed to being pressed in the last update
        bool actionOnPress() { return actionIsPressed() && actionChanged(); }
        //! True if action changed to being released in the last update
        bool actionOnRelease() { return actionIsReleased() && actionChanged(); };
        //! True if the action is currently being pressed
        bool actionIsPressed() { return mPressed; }
        //! True if the action is currently not being pressed
        bool actionIsReleased() { return !mPressed; }
        //! True if the action changed state in the last update
        bool actionChanged() { return mChanged; }

        OpenXRAction mAction;
        bool mPressed = false;
        bool mChanged = false;
    };

    //! Wrapper around float type input actions, ignoring all subactions
    struct OpenXRAction_Float
    {
        OpenXRAction_Float(OpenXRAction action);
        operator XrAction() { return mAction; }

        void update();

        //! Current value of the control, from -1.f to 1.f for sticks or 0.f to 1.f for controls
        float value() { return mValue; }

        OpenXRAction mAction;
        float mValue = 0.f;
    };

    struct OpenXRInput
    {
        using Actions = MWInput::InputManager::Actions;

        enum SubAction : signed
        {
            NONE = -1, //!< Used to ignore subaction or when action has no subaction. Not a valid input to createAction().
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

        OpenXRInput(osg::ref_ptr<OpenXRManager> XR);

        OpenXRAction createAction(XrActionType actionType, const std::string& actionName, const std::string& localName, const std::vector<SubAction>& subActions = {});

        XrActionSet createActionSet(void);

        XrPath subactionPath(SubAction subAction);

        void updateControls();
        bool nextActionEvent(OpenXRActionEvent& action);
        PoseSet getHandPoses(int64_t time, TrackedSpace space);

        osg::ref_ptr<OpenXRManager> mXR;
        SubActionPaths mSubactionPath;
        XrActionSet mActionSet = XR_NULL_HANDLE;

        ControllerActionPaths mSelectPath;
        ControllerActionPaths mSqueezeValuePath;
        ControllerActionPaths mSqueezeClickPath;
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
        ControllerActionPaths mTriggerClickPath;
        ControllerActionPaths mTriggerValuePath;

        OpenXRAction_Bool mGameMenu;
        OpenXRAction_Bool mInventory;
        OpenXRAction_Bool mActivate;
        OpenXRAction_Bool mUse;
        OpenXRAction_Bool mJump;
        OpenXRAction_Bool mWeapon;
        OpenXRAction_Bool mSpell;
        OpenXRAction_Bool mCycleSpellLeft;
        OpenXRAction_Bool mCycleSpellRight;
        OpenXRAction_Bool mCycleWeaponLeft;
        OpenXRAction_Bool mCycleWeaponRight;
        OpenXRAction_Bool mSneak;
        OpenXRAction_Bool mQuickMenu;
        OpenXRAction_Float mLookLeftRight;
        OpenXRAction_Float mMoveForwardBackward;
        OpenXRAction_Float mMoveLeftRight;
        //OpenXRAction mUnused;
        //OpenXRAction mScreenshot;
        //OpenXRAction mConsole;
        //OpenXRAction mMoveLeft;
        //OpenXRAction mMoveRight;
        //OpenXRAction mMoveForward;
        //OpenXRAction mMoveBackward;
        //OpenXRAction mAutoMove;
        //OpenXRAction mRest;
        //OpenXRAction mJournal;
        //OpenXRAction mRun;
        //OpenXRAction mToggleSneak;
        //OpenXRAction mAlwaysRun;
        //OpenXRAction mQuickSave;
        //OpenXRAction mQuickLoad;
        //OpenXRAction mToggleWeapon;
        //OpenXRAction mToggleSpell;
        //OpenXRAction mTogglePOV;
        //OpenXRAction mQuickKey1;
        //OpenXRAction mQuickKey2;
        //OpenXRAction mQuickKey3;
        //OpenXRAction mQuickKey4;
        //OpenXRAction mQuickKey5;
        //OpenXRAction mQuickKey6;
        //OpenXRAction mQuickKey7;
        //OpenXRAction mQuickKey8;
        //OpenXRAction mQuickKey9;
        //OpenXRAction mQuickKey10;
        //OpenXRAction mQuickKeysMenu;
        //OpenXRAction mToggleHUD;
        //OpenXRAction mToggleDebug;
        //OpenXRAction mLookUpDown;
        //OpenXRAction mZoomIn;
        //OpenXRAction mZoomOut;

        //! Needed to access all the actions that don't fit on the controllers
        OpenXRAction_Bool mActionsMenu;
        //! Economize buttons by accessing spell actions and weapon actions on the same keys, but with/without this modifier
        OpenXRAction_Bool mSpellModifier;

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


        std::deque<OpenXRActionEvent> mActionEvents{};
    };

    XrActionSet
        OpenXRInput::createActionSet()
    {
        XrActionSet actionSet = XR_NULL_HANDLE;
        XrActionSetCreateInfo createInfo{ XR_TYPE_ACTION_SET_CREATE_INFO };
        strcpy_s(createInfo.actionSetName, "gameplay");
        strcpy_s(createInfo.localizedActionSetName, "Gameplay");
        createInfo.priority = 0;
        CHECK_XRCMD(xrCreateActionSet(mXR->impl().mInstance, &createInfo, &actionSet));
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
        osg::ref_ptr<OpenXRManager> XR,
        XrAction action,
        XrActionType actionType,
        const std::string& actionName,
        const std::string& localName)
        : mXR(XR)
        , mAction(action)
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
        XrActionStateGetInfo getInfo{ XR_TYPE_ACTION_STATE_GET_INFO };
        getInfo.action = mAction;
        getInfo.subactionPath = subactionPath;

        XrActionStateFloat xrValue{ XR_TYPE_ACTION_STATE_FLOAT };
        CHECK_XRCMD(xrGetActionStateFloat(mXR->impl().mSession, &getInfo, &xrValue));

        if (xrValue.isActive)
            value = xrValue.currentState;
        return xrValue.isActive;
    }

    bool OpenXRAction::getBool(XrPath subactionPath, bool& value)
    {
        XrActionStateGetInfo getInfo{ XR_TYPE_ACTION_STATE_GET_INFO };
        getInfo.action = mAction;
        getInfo.subactionPath = subactionPath;

        XrActionStateBoolean xrValue{ XR_TYPE_ACTION_STATE_BOOLEAN };
        CHECK_XRCMD(xrGetActionStateBoolean(mXR->impl().mSession, &getInfo, &xrValue));

        if (xrValue.isActive)
            value = xrValue.currentState;
        return xrValue.isActive;
    }

    // Pose action only checks if the pose is active or not
    bool OpenXRAction::getPose(XrPath subactionPath)
    {
        XrActionStateGetInfo getInfo{ XR_TYPE_ACTION_STATE_GET_INFO };
        getInfo.action = mAction;
        getInfo.subactionPath = subactionPath;

        XrActionStatePose xrValue{ XR_TYPE_ACTION_STATE_POSE };
        CHECK_XRCMD(xrGetActionStatePose(mXR->impl().mSession, &getInfo, &xrValue));

        return xrValue.isActive;
    }

    bool OpenXRAction::applyHaptics(XrPath subactionPath, float amplitude)
    {
        XrHapticVibration vibration{ XR_TYPE_HAPTIC_VIBRATION };
        vibration.amplitude = amplitude;
        vibration.duration = XR_MIN_HAPTIC_DURATION;
        vibration.frequency = XR_FREQUENCY_UNSPECIFIED;

        XrHapticActionInfo hapticActionInfo{ XR_TYPE_HAPTIC_ACTION_INFO };
        hapticActionInfo.action = mAction;
        hapticActionInfo.subactionPath = subactionPath;
        CHECK_XRCMD(xrApplyHapticFeedback(mXR->impl().mSession, &hapticActionInfo, (XrHapticBaseHeader*)&vibration));
        return true;
    }

    OpenXRAction_Bool::OpenXRAction_Bool(
        OpenXRAction action)
        : mAction(std::move(action))
    {

    }

    void
        OpenXRAction_Bool::update()
    {
        bool old = mPressed;
        mAction.getBool(0, mPressed);
        mChanged = mPressed != old;
    }

    OpenXRAction_Float::OpenXRAction_Float(
        OpenXRAction action)
        : mAction(std::move(action))
    {

    }

    void
        OpenXRAction_Float::update()
    {
        mAction.getFloat(0, mValue);
    }

    OpenXRInput::ControllerActionPaths
        OpenXRInput::generateControllerActionPaths(
            const std::string& controllerAction)
    {
        ControllerActionPaths actionPaths;

        std::string left = std::string("/user/hand/left") + controllerAction;
        std::string right = std::string("/user/hand/right") + controllerAction;
        std::string pad = std::string("/user/gamepad") + controllerAction;

        CHECK_XRCMD(xrStringToPath(mXR->impl().mInstance, left.c_str(), &actionPaths[LEFT_HAND]));
        CHECK_XRCMD(xrStringToPath(mXR->impl().mInstance, right.c_str(), &actionPaths[RIGHT_HAND]));
        CHECK_XRCMD(xrStringToPath(mXR->impl().mInstance, pad.c_str(), &actionPaths[GAMEPAD]));

        return actionPaths;
    }

    OpenXRInput::OpenXRInput(
        osg::ref_ptr<OpenXRManager> XR)
        : mXR(XR)
        , mSubactionPath{ {
                generateXrPath("/user/hand/left"),
                generateXrPath("/user/hand/right"),
                generateXrPath("/user/gamepad"),
                generateXrPath("/user/head"),
                generateXrPath("/user")
            } }
        , mActionSet(createActionSet())
        , mSelectPath(generateControllerActionPaths("/input/select/click"))
        , mSqueezeValuePath(generateControllerActionPaths("/input/squeeze/value"))
        , mSqueezeClickPath(generateControllerActionPaths("/input/squeeze/click"))
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
        , mTriggerClickPath(generateControllerActionPaths("/input/trigger/click"))
        , mTriggerValuePath(generateControllerActionPaths("/input/trigger/value"))
        , mGameMenu(std::move(createAction(XR_ACTION_TYPE_BOOLEAN_INPUT, "game_menu", "GameMenu", { })))
        , mInventory(std::move(createAction(XR_ACTION_TYPE_BOOLEAN_INPUT, "inventory", "Inventory", { })))
        , mActivate(std::move(createAction(XR_ACTION_TYPE_BOOLEAN_INPUT, "activate", "Activate", { })))
        , mUse(std::move(createAction(XR_ACTION_TYPE_BOOLEAN_INPUT, "use", "Use", { })))
        , mJump(std::move(createAction(XR_ACTION_TYPE_BOOLEAN_INPUT, "jump", "Jump", { })))
        , mWeapon(std::move(createAction(XR_ACTION_TYPE_BOOLEAN_INPUT, "weapon", "Weapon", { })))
        , mSpell(std::move(createAction(XR_ACTION_TYPE_BOOLEAN_INPUT, "spell", "Spell", { })))
        , mCycleSpellLeft(std::move(createAction(XR_ACTION_TYPE_BOOLEAN_INPUT, "cycle_spell_left", "CycleSpellLeft", { })))
        , mCycleSpellRight(std::move(createAction(XR_ACTION_TYPE_BOOLEAN_INPUT, "cycle_spell_right", "CycleSpellRight", { })))
        , mCycleWeaponLeft(std::move(createAction(XR_ACTION_TYPE_BOOLEAN_INPUT, "cycle_weapon_left", "CycleWeaponLeft", { })))
        , mCycleWeaponRight(std::move(createAction(XR_ACTION_TYPE_BOOLEAN_INPUT, "cycle_weapon_right", "CycleWeaponRight", { })))
        , mSneak(std::move(createAction(XR_ACTION_TYPE_BOOLEAN_INPUT, "sneak", "Sneak", { })))
        , mQuickMenu(std::move(createAction(XR_ACTION_TYPE_BOOLEAN_INPUT, "quick_menu", "QuickMenu", { })))
        , mLookLeftRight(std::move(createAction(XR_ACTION_TYPE_FLOAT_INPUT, "look_left_right", "LookLeftRight", { })))
        , mMoveForwardBackward(std::move(createAction(XR_ACTION_TYPE_FLOAT_INPUT, "move_forward_backward", "MoveForwardBackward", { })))
        , mMoveLeftRight(std::move(createAction(XR_ACTION_TYPE_FLOAT_INPUT, "move_left_right", "MoveLeftRight", { })))
        , mActionsMenu(std::move(createAction(XR_ACTION_TYPE_BOOLEAN_INPUT, "actions_menu", "Actions Menu", { })))
        , mSpellModifier(std::move(createAction(XR_ACTION_TYPE_BOOLEAN_INPUT, "spell_modifier", "Spell Modifier", { })))
        , mHandPoseAction(std::move(createAction(XR_ACTION_TYPE_POSE_INPUT, "hand_pose", "Hand Pose", { LEFT_HAND, RIGHT_HAND })))
        , mHapticsAction(std::move(createAction(XR_ACTION_TYPE_VIBRATION_OUTPUT, "vibrate_hand", "Vibrate Hand", { LEFT_HAND, RIGHT_HAND })))
    {
        { // Set up default bindings for the oculus
            XrPath oculusTouchInteractionProfilePath;
            CHECK_XRCMD(
                xrStringToPath(XR->impl().mInstance, "/interaction_profiles/oculus/touch_controller", &oculusTouchInteractionProfilePath));
            std::vector<XrActionSuggestedBinding> bindings{ {
                {mHandPoseAction, mPosePath[LEFT_HAND]},
                {mHandPoseAction, mPosePath[RIGHT_HAND]},
                {mHapticsAction, mHapticPath[LEFT_HAND]},
                {mHapticsAction, mHapticPath[RIGHT_HAND]},
                {mLookLeftRight, mThumbstickXPath[RIGHT_HAND]},
                {mMoveLeftRight, mThumbstickXPath[LEFT_HAND]},
                {mMoveForwardBackward, mThumbstickYPath[LEFT_HAND]},
                {mActivate, mSqueezeValuePath[RIGHT_HAND]},
                {mUse, mTriggerValuePath[RIGHT_HAND]},
                {mJump, mTriggerValuePath[LEFT_HAND]},
                {mWeapon, mAPath[RIGHT_HAND]},
                {mSpell, mAPath[RIGHT_HAND]},
                {mCycleSpellLeft, mThumbstickClickPath[LEFT_HAND]},
                {mCycleSpellRight, mThumbstickClickPath[RIGHT_HAND]},
                {mCycleWeaponLeft, mThumbstickClickPath[LEFT_HAND]},
                {mCycleWeaponRight, mThumbstickClickPath[RIGHT_HAND]},
                {mSneak, mXPath[LEFT_HAND]},
                {mInventory, mBPath[RIGHT_HAND]},
                {mQuickMenu, mYPath[LEFT_HAND]},
                {mSpellModifier, mSqueezeValuePath[LEFT_HAND]},
                {mGameMenu, mMenuClickPath[LEFT_HAND]},
              } };
            XrInteractionProfileSuggestedBinding suggestedBindings{ XR_TYPE_INTERACTION_PROFILE_SUGGESTED_BINDING };
            suggestedBindings.interactionProfile = oculusTouchInteractionProfilePath;
            suggestedBindings.suggestedBindings = bindings.data();
            suggestedBindings.countSuggestedBindings = (uint32_t)bindings.size();
            CHECK_XRCMD(xrSuggestInteractionProfileBindings(XR->impl().mInstance, &suggestedBindings));

            /*
            mSpellModifier; //  L-Squeeze
            mActivate; // R-Squeeze
            mUse; // R-Trigger
            mJump; // L-Trigger. L-trigger has value, can be used to make measured jumps
            mWeapon; // A
            mSpell; // A + SpellModifier
            mRun; // Based on movement thumbstick value: broken line ( 0 (stand), 0.5 (walk), 1.0 (run) ). Remember to scale fatigue use.
            mCycleSpellLeft; // L-ThumbstickClick + SpellModifier
            mCycleSpellRight; // R-ThumbstickClick + SpellModifier
            mCycleWeaponLeft; // L-ThumbstickClick
            mCycleWeaponRight; // R-ThumbstickClick
            mSneak; // X
            mInventory; // B
            mQuickMenu; // Y
            mGameMenu; // Menu
            */

        }

        { // Set up action spaces
            XrActionSpaceCreateInfo createInfo{ XR_TYPE_ACTION_SPACE_CREATE_INFO };
            createInfo.action = mHandPoseAction;
            createInfo.poseInActionSpace.orientation.w = 1.f;
            createInfo.subactionPath = mSubactionPath[LEFT_HAND];
            CHECK_XRCMD(xrCreateActionSpace(XR->impl().mSession, &createInfo, &mHandSpace[LEFT_HAND]));
            createInfo.subactionPath = mSubactionPath[RIGHT_HAND];
            CHECK_XRCMD(xrCreateActionSpace(XR->impl().mSession, &createInfo, &mHandSpace[RIGHT_HAND]));
        }

        { // Set up the action set
            XrSessionActionSetsAttachInfo attachInfo{ XR_TYPE_SESSION_ACTION_SETS_ATTACH_INFO };
            attachInfo.countActionSets = 1;
            attachInfo.actionSets = &mActionSet;
            CHECK_XRCMD(xrAttachSessionActionSets(XR->impl().mSession, &attachInfo));
        }
    };

    OpenXRAction
        OpenXRInput::createAction(
            XrActionType actionType,
            const std::string& actionName,
            const std::string& localName,
            const std::vector<SubAction>& subActions)
    {
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
        return OpenXRAction(mXR, action, actionType, actionName, localName);
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
        if (!mXR->impl().mSessionRunning)
            return;


        const XrActiveActionSet activeActionSet{ mActionSet, XR_NULL_PATH };
        XrActionsSyncInfo syncInfo{ XR_TYPE_ACTIONS_SYNC_INFO };
        syncInfo.countActiveActionSets = 1;
        syncInfo.activeActionSets = &activeActionSet;
        CHECK_XRCMD(xrSyncActions(mXR->impl().mSession, &syncInfo));

        mGameMenu.update();
        mInventory.update();
        mActivate.update();
        mUse.update();
        mJump.update();
        mWeapon.update();
        mSpell.update();
        mCycleSpellLeft.update();
        mCycleSpellRight.update();
        mCycleWeaponLeft.update();
        mCycleWeaponRight.update();
        mSneak.update();
        mQuickMenu.update();
        mLookLeftRight.update();
        mMoveForwardBackward.update();
        mMoveLeftRight.update();
        mActionsMenu.update();
        mSpellModifier.update();

        // This set of actions only care about on-press
        if (mActionsMenu.actionOnPress())
        {
            mActionEvents.emplace_back(OpenXRActionEvent{ MWInput::InputManager::A_QuickKeysMenu, true });
        }
        if (mGameMenu.actionOnPress())
        {
            mActionEvents.emplace_back(OpenXRActionEvent{ MWInput::InputManager::A_GameMenu, true });
        }
        if (mInventory.actionOnPress())
        {
            mActionEvents.emplace_back(OpenXRActionEvent{ MWInput::InputManager::A_Inventory, true });
        }
        if (mActivate.actionOnPress())
        {
            mActionEvents.emplace_back(OpenXRActionEvent{ MWInput::InputManager::A_Activate, true });
        }
        if (mUse.actionChanged())
        {
            mActionEvents.emplace_back(OpenXRActionEvent{ MWInput::InputManager::A_Use, mUse.actionOnPress() });
        }
        if (mJump.actionOnPress())
        {
            mActionEvents.emplace_back(OpenXRActionEvent{ MWInput::InputManager::A_Jump, true });
        }
        if (mSneak.actionOnPress())
        {
            mActionEvents.emplace_back(OpenXRActionEvent{ MWInput::InputManager::A_Sneak, true });
        }
        if (mSneak.actionOnRelease())
        {
            mActionEvents.emplace_back(OpenXRActionEvent{ MWInput::InputManager::A_Sneak, false });
        }
        if (mQuickMenu.actionOnPress())
        {
            mActionEvents.emplace_back(OpenXRActionEvent{ MWInput::InputManager::A_QuickMenu, true });
        }

        // Weapon/Spell actions
        if (mWeapon.actionOnPress() && !mSpellModifier.actionIsPressed())
        {
            mActionEvents.emplace_back(OpenXRActionEvent{ MWInput::InputManager::A_ToggleWeapon, true });
        }
        if (mCycleWeaponLeft.actionOnPress() && !mSpellModifier.actionIsPressed())
        {
            mActionEvents.emplace_back(OpenXRActionEvent{ MWInput::InputManager::A_CycleWeaponLeft, true });
        }
        if (mCycleWeaponRight.actionOnPress() && !mSpellModifier.actionIsPressed())
        {
            mActionEvents.emplace_back(OpenXRActionEvent{ MWInput::InputManager::A_CycleWeaponRight, true });
        }
        if (mSpell.actionOnPress() && mSpellModifier.actionIsPressed())
        {
            mActionEvents.emplace_back(OpenXRActionEvent{ MWInput::InputManager::A_ToggleSpell, true });
        }
        if (mCycleSpellLeft.actionOnPress() && mSpellModifier.actionIsPressed())
        {
            mActionEvents.emplace_back(OpenXRActionEvent{ MWInput::InputManager::A_CycleSpellLeft, true });
        }
        if (mCycleSpellRight.actionOnPress() && mSpellModifier.actionIsPressed())
        {
            mActionEvents.emplace_back(OpenXRActionEvent{ MWInput::InputManager::A_CycleSpellRight, true });
        }


        float lookLeftRight = mLookLeftRight.value();
        float moveLeftRight = mMoveLeftRight.value();
        float moveForwardBackward = mMoveForwardBackward.value();

        mActionEvents.emplace_back(OpenXRActionEvent{ MWInput::InputManager::A_LookLeftRight, false, lookLeftRight });
        mActionEvents.emplace_back(OpenXRActionEvent{ MWInput::InputManager::A_MoveLeftRight, false, moveLeftRight });
        mActionEvents.emplace_back(OpenXRActionEvent{ MWInput::InputManager::A_MoveForwardBackward, false, moveForwardBackward });
    }

    XrPath OpenXRInput::generateXrPath(const std::string& path)
    {
        XrPath xrpath = 0;
        CHECK_XRCMD(xrStringToPath(mXR->impl().mInstance, path.c_str(), &xrpath));
        return xrpath;
    }

    bool OpenXRInput::nextActionEvent(OpenXRActionEvent& action)
    {
        action = {};

        if (mActionEvents.empty())
            return false;

        action = mActionEvents.front();
        mActionEvents.pop_front();
        return true;

    }

    PoseSet
        OpenXRInput::getHandPoses(
            int64_t time,
            TrackedSpace space)
    {
        PoseSet handPoses{};
        XrSpace referenceSpace = XR_NULL_HANDLE;
        if(space == TrackedSpace::STAGE)
            referenceSpace = mXR->impl().mReferenceSpaceStage;
        if(space == TrackedSpace::VIEW)
            referenceSpace = mXR->impl().mReferenceSpaceView;

        XrSpaceLocation location{ XR_TYPE_SPACE_LOCATION };
        XrSpaceVelocity velocity{ XR_TYPE_SPACE_VELOCITY };
        location.next = &velocity;
        CHECK_XRCMD(xrLocateSpace(mHandSpace[(int)Chirality::LEFT_HAND], referenceSpace, time, &location));

        handPoses[(int)Chirality::LEFT_HAND] = MWVR::Pose{
            osg::fromXR(location.pose.position),
            osg::fromXR(location.pose.orientation),
            osg::fromXR(velocity.linearVelocity)
        };

        CHECK_XRCMD(xrLocateSpace(mHandSpace[(int)Chirality::RIGHT_HAND], referenceSpace, time, &location));
        handPoses[(int)Chirality::RIGHT_HAND] = MWVR::Pose{
            osg::fromXR(location.pose.position),
            osg::fromXR(location.pose.orientation),
            osg::fromXR(velocity.linearVelocity)
        };

        return handPoses;
    }


    PoseSet OpenXRInputManager::getHandPoses(int64_t time, TrackedSpace space)
    {
        return mXRInput->getHandPoses(time, space);
    }

    OpenXRInputManager::OpenXRInputManager(
        SDL_Window* window,
        osg::ref_ptr<OpenXRViewer> viewer, 
        osg::ref_ptr<osgViewer::ScreenCaptureHandler> screenCaptureHandler, 
        osgViewer::ScreenCaptureHandler::CaptureOperation* screenCaptureOperation, 
        const std::string& userFile, 
        bool userFileExists, 
        const std::string& userControllerBindingsFile, 
        const std::string& controllerBindingsFile, 
        bool grab)
        : MWInput::InputManager(
            window, 
            viewer->mViewer, 
            screenCaptureHandler,
            screenCaptureOperation,
            userFile,
            userFileExists,
            userControllerBindingsFile,
            controllerBindingsFile,
            grab)
        , mXRInput(new OpenXRInput(viewer->mXR))
    {
        // VR mode has no concept of these
        mControlSwitch["vanitymode"] = false;
        mGuiCursorEnabled = false;
    }

    OpenXRInputManager::~OpenXRInputManager()
    {
    }

    void OpenXRInputManager::changeInputMode(bool mode)
    {
        // VR mode has no concept of these
        mGuiCursorEnabled = false;
        MWInput::InputManager::changeInputMode(mode);
        MWBase::Environment::get().getWindowManager()->showCrosshair(false);
        MWBase::Environment::get().getWindowManager()->setCursorVisible(false);
    }

    void OpenXRInputManager::update(
        float dt,
        bool disableControls,
        bool disableEvents)
    {

        mXRInput->updateControls();

        auto* session = MWBase::Environment::get().getXRSession();

        OpenXRActionEvent event{};
        while (mXRInput->nextActionEvent(event))
        {
            //Log(Debug::Verbose) << "ActionEvent action=" << event.action << " onPress=" << event.onPress;
            processEvent(event);
        }

        MWInput::InputManager::update(dt, disableControls, disableEvents);

        bool guiMode = MWBase::Environment::get().getWindowManager()->isGuiMode();
        session->showMenu(guiMode);
    }

    void OpenXRInputManager::processEvent(const OpenXRActionEvent& event)
    {
        switch (event.action)
        {
        case A_GameMenu:
                Log(Debug::Verbose) << "A_GameMenu";
                toggleMainMenu();
                // Explicitly request position update here so that the player can move the menu
                // using the menu key when the menu can't be toggled.
                MWBase::Environment::get().getXRSession()->updateMenuPosition();
                break;
        case A_Screenshot:
            screenshot();
            break;
        case A_Inventory:
            toggleInventory();
            break;
        case A_Console:
            toggleConsole();
            break;
        case A_Activate:
            resetIdleTime();
            activate();
            break;
        // TODO: Movement
        //case A_MoveLeft:
        //case A_MoveRight:
        //case A_MoveForward:
        //case A_MoveBackward:
        //    handleGuiArrowKey(action);
        //    break;
        case A_Journal:
            toggleJournal();
            break;
        case A_AutoMove:
            toggleAutoMove();
            break;
        case A_AlwaysRun:
            toggleWalking();
            break;
        case A_ToggleWeapon:
            toggleWeapon();
            break;
        case A_Rest:
            rest();
            break;
        case A_ToggleSpell:
            toggleSpell();
            break;
        case A_QuickKey1:
            quickKey(1);
            break;
        case A_QuickKey2:
            quickKey(2);
            break;
        case A_QuickKey3:
            quickKey(3);
            break;
        case A_QuickKey4:
            quickKey(4);
            break;
        case A_QuickKey5:
            quickKey(5);
            break;
        case A_QuickKey6:
            quickKey(6);
            break;
        case A_QuickKey7:
            quickKey(7);
            break;
        case A_QuickKey8:
            quickKey(8);
            break;
        case A_QuickKey9:
            quickKey(9);
            break;
        case A_QuickKey10:
            quickKey(10);
            break;
        case A_QuickKeysMenu:
            showQuickKeysMenu();
            break;
        case A_ToggleHUD:
            MWBase::Environment::get().getWindowManager()->toggleHud();
            break;
        case A_ToggleDebug:
            MWBase::Environment::get().getWindowManager()->toggleDebugWindow();
            break;
            // Does not apply in VR
        //case A_ZoomIn:
        //    if (mControlSwitch["playerviewswitch"] && mControlSwitch["playercontrols"] && !MWBase::Environment::get().getWindowManager()->isGuiMode())
        //        MWBase::Environment::get().getWorld()->setCameraDistance(ZOOM_SCALE, true, true);
        //    break;
        //case A_ZoomOut:
        //    if (mControlSwitch["playerviewswitch"] && mControlSwitch["playercontrols"] && !MWBase::Environment::get().getWindowManager()->isGuiMode())
        //        MWBase::Environment::get().getWorld()->setCameraDistance(-ZOOM_SCALE, true, true);
        //    break;
        case A_QuickSave:
            quickSave();
            break;
        case A_QuickLoad:
            quickLoad();
            break;
        case A_CycleSpellLeft:
            if (checkAllowedToUseItems() && MWBase::Environment::get().getWindowManager()->isAllowed(MWGui::GW_Magic))
                MWBase::Environment::get().getWindowManager()->cycleSpell(false);
            break;
        case A_CycleSpellRight:
            if (checkAllowedToUseItems() && MWBase::Environment::get().getWindowManager()->isAllowed(MWGui::GW_Magic))
                MWBase::Environment::get().getWindowManager()->cycleSpell(true);
            break;
        case A_CycleWeaponLeft:
            if (checkAllowedToUseItems() && MWBase::Environment::get().getWindowManager()->isAllowed(MWGui::GW_Inventory))
                MWBase::Environment::get().getWindowManager()->cycleWeapon(false);
            break;
        case A_CycleWeaponRight:
            if (checkAllowedToUseItems() && MWBase::Environment::get().getWindowManager()->isAllowed(MWGui::GW_Inventory))
                MWBase::Environment::get().getWindowManager()->cycleWeapon(true);
            break;
        case A_Sneak:
            if (mSneakToggles)
            {
                toggleSneaking();
            }
            break;
        case A_LookLeftRight:
            mInputBinder->getChannel(A_LookLeftRight)->setValue(event.value / 2.f + 0.5f);
            break;
        case A_MoveLeftRight:
            mInputBinder->getChannel(A_MoveLeftRight)->setValue(event.value / 2.f + 0.5f);
            break;
        case A_MoveForwardBackward:
            mInputBinder->getChannel(A_MoveForwardBackward)->setValue(-event.value / 2.f + 0.5f);
            break;
        case A_Jump:
            mAttemptJump = true;
            break;
        case A_Use:
            //MWMechanics::DrawState_ state = MWBase::Environment::get().getWorld()->getPlayer().getDrawState();
            //mPlayer->setAttackingOrSpell(currentValue != 0 && state != MWMechanics::DrawState_Nothing);
            mInputBinder->getChannel(A_Use)->setValue(event.onPress);
            break;
        default:
            Log(Debug::Warning) << "Unhandled XR action " << event.action;
        }
    }
}
