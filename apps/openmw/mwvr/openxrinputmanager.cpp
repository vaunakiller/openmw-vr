#include "openxrinputmanager.hpp"
#include "openxrmanager.hpp"
#include "openxrmanagerimpl.hpp"
#include "../mwinput/inputmanagerimp.hpp"

#include <components/debug/debuglog.hpp>
#include <components/sdlutil/sdlgraphicswindow.hpp>

#include <openxr/openxr.h>

#include <osg/Camera>

#include <vector>
#include <array>
#include <iostream>

namespace MWVR
{
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

    struct OpenXRInputManagerImpl
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

        OpenXRInputManagerImpl(osg::ref_ptr<OpenXRManager> XR);

        OpenXRAction createAction(XrActionType actionType, const std::string& actionName, const std::string& localName, const std::vector<SubAction>& subActions = {});

        XrActionSet createActionSet(void);

        void updateHandTracking();

        XrPath subactionPath(SubAction subAction);

        void updateControls();
        void updateHead();
        void updateHands();

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
    };

    XrActionSet
        OpenXRInputManagerImpl::createActionSet()
    {
        XrActionSet actionSet = XR_NULL_HANDLE;
        XrActionSetCreateInfo createInfo{ XR_TYPE_ACTION_SET_CREATE_INFO };
        strcpy_s(createInfo.actionSetName, "gameplay");
        strcpy_s(createInfo.localizedActionSetName, "Gameplay");
        createInfo.priority = 0;
        CHECK_XRCMD(xrCreateActionSet(mXR->mPrivate->mInstance, &createInfo, &actionSet));
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
        CHECK_XRCMD(xrGetActionStateFloat(mXR->mPrivate->mSession, &getInfo, &xrValue));

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
        CHECK_XRCMD(xrGetActionStateBoolean(mXR->mPrivate->mSession, &getInfo, &xrValue));

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
        CHECK_XRCMD(xrGetActionStatePose(mXR->mPrivate->mSession, &getInfo, &xrValue));

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
        CHECK_XRCMD(xrApplyHapticFeedback(mXR->mPrivate->mSession, &hapticActionInfo, (XrHapticBaseHeader*)&vibration));
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

    OpenXRInputManagerImpl::ControllerActionPaths
        OpenXRInputManagerImpl::generateControllerActionPaths(
            const std::string& controllerAction)
    {
        ControllerActionPaths actionPaths;

        std::string left = std::string("/user/hand/left") + controllerAction;
        std::string right = std::string("/user/hand/right") + controllerAction;
        std::string pad = std::string("/user/gamepad") + controllerAction;

        CHECK_XRCMD(xrStringToPath(mXR->mPrivate->mInstance, left.c_str(), &actionPaths[LEFT_HAND]));
        CHECK_XRCMD(xrStringToPath(mXR->mPrivate->mInstance, right.c_str(), &actionPaths[RIGHT_HAND]));
        CHECK_XRCMD(xrStringToPath(mXR->mPrivate->mInstance, pad.c_str(), &actionPaths[GAMEPAD]));

        return actionPaths;
    }

    OpenXRInputManagerImpl::OpenXRInputManagerImpl(
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
        , mPosePath(generateControllerActionPaths("/input/grip/pose"))
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
        , mTriggerClickPath(generateControllerActionPaths("/input/trigger/click"))
        , mActionsMenu(std::move(createAction(XR_ACTION_TYPE_BOOLEAN_INPUT, "actions_menu", "Actions Menu", { })))
        , mSpellModifier(std::move(createAction(XR_ACTION_TYPE_BOOLEAN_INPUT, "spell_modifier", "Spell Modifier", { })))
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
        , mHandPoseAction(std::move(createAction(XR_ACTION_TYPE_POSE_INPUT, "hand_pose", "Hand Pose", { LEFT_HAND, RIGHT_HAND })))
        , mHapticsAction(std::move(createAction(XR_ACTION_TYPE_VIBRATION_OUTPUT, "vibrate_hand", "Vibrate Hand", { LEFT_HAND, RIGHT_HAND })))
    {



        //{ mHandPoseAction, mPosePath[LEFT_HAND]},
        //{ mHandPoseAction, mPosePath[RIGHT_HAND] },
        //{ mHapticsAction, mHapticPath[LEFT_HAND] },
        //{ mHapticsAction, mHapticPath[RIGHT_HAND] },

        //{ mLookLeftRight, mThumbstickXPath[RIGHT_HAND] },
        //{ mMoveLeftRight, mThumbstickXPath[LEFT_HAND] },
        //{ mMoveForwardBackward, mThumbstickYPath[LEFT_HAND] },
        //{ mActivate, mSqueezeClickPath[RIGHT_HAND] },
        //{ mUse, mTriggerClickPath[RIGHT_HAND] },
        //{ mJump, mTriggerValuePath[LEFT_HAND] },
        //{ mWeapon, mAPath[RIGHT_HAND] },
        //{ mSpell, mAPath[RIGHT_HAND] },
        //{ mCycleSpellLeft, mThumbstickClickPath[LEFT_HAND] },
        //{ mCycleSpellRight, mThumbstickClickPath[RIGHT_HAND] },
        //{ mCycleWeaponLeft, mThumbstickClickPath[LEFT_HAND] },
        //{ mCycleWeaponRight, mThumbstickClickPath[RIGHT_HAND] },
        //{ mSneak, mXPath[LEFT_HAND] },
        //{ mInventory, mBPath[RIGHT_HAND] },
        //{ mQuickMenu, mYPath[LEFT_HAND] },
        //{ mSpellModifier, mSqueezeClickPath[LEFT_HAND] },
        //{ mGameMenu, mMenuClickPath[LEFT_HAND] },

        // There are not enough actions on controllers to assign everything.
        //mUnused = std::move(createAction(XR_ACTION_TYPE_FLOAT_INPUT, "Unused", "Unused", { }));
        //mScreenshot = std::move(createAction(XR_ACTION_TYPE_FLOAT_INPUT, "Screenshot", "Screenshot", { }));
        //mConsole = std::move(createAction(XR_ACTION_TYPE_FLOAT_INPUT, "Console", "Console", { }));
        //mMoveLeft = std::move(createAction(XR_ACTION_TYPE_FLOAT_INPUT, "MoveLeft", "MoveLeft", { }));
        //mMoveRight = std::move(createAction(XR_ACTION_TYPE_FLOAT_INPUT, "MoveRight", "MoveRight", { }));
        //mMoveForward = std::move(createAction(XR_ACTION_TYPE_FLOAT_INPUT, "MoveForward", "MoveForward", { }));
        //mMoveBackward = std::move(createAction(XR_ACTION_TYPE_FLOAT_INPUT, "MoveBackward", "MoveBackward", { }));
        //mAutoMove = std::move(createAction(XR_ACTION_TYPE_FLOAT_INPUT, "AutoMove", "AutoMove", { }));
        //mRest = std::move(createAction(XR_ACTION_TYPE_FLOAT_INPUT, "Rest", "Rest", { }));
        //mJournal = std::move(createAction(XR_ACTION_TYPE_FLOAT_INPUT, "Journal", "Journal", { }));
        //mRun = std::move(createAction(XR_ACTION_TYPE_FLOAT_INPUT, "Run", "Run", { }));
        //mAlwaysRun = std::move(createAction(XR_ACTION_TYPE_FLOAT_INPUT, "AlwaysRun", "AlwaysRun", { }));
        //mQuickSave = std::move(createAction(XR_ACTION_TYPE_FLOAT_INPUT, "QuickSave", "QuickSave", { }));
        //mQuickLoad = std::move(createAction(XR_ACTION_TYPE_FLOAT_INPUT, "QuickLoad", "QuickLoad", { }));
        //mToggleWeapon = std::move(createAction(XR_ACTION_TYPE_FLOAT_INPUT, "ToggleWeapon", "ToggleWeapon", { }));
        //mToggleSpell = std::move(createAction(XR_ACTION_TYPE_FLOAT_INPUT, "ToggleSpell", "ToggleSpell", { }));
        //mTogglePOV = std::move(createAction(XR_ACTION_TYPE_FLOAT_INPUT, "TogglePOV", "TogglePOV", { }));
        //mQuickKey1 = std::move(createAction(XR_ACTION_TYPE_FLOAT_INPUT, "QuickKey1", "QuickKey1", { }));
        //mQuickKey2 = std::move(createAction(XR_ACTION_TYPE_FLOAT_INPUT, "QuickKey2", "QuickKey2", { }));
        //mQuickKey3 = std::move(createAction(XR_ACTION_TYPE_FLOAT_INPUT, "QuickKey3", "QuickKey3", { }));
        //mQuickKey4 = std::move(createAction(XR_ACTION_TYPE_FLOAT_INPUT, "QuickKey4", "QuickKey4", { }));
        //mQuickKey5 = std::move(createAction(XR_ACTION_TYPE_FLOAT_INPUT, "QuickKey5", "QuickKey5", { }));
        //mQuickKey6 = std::move(createAction(XR_ACTION_TYPE_FLOAT_INPUT, "QuickKey6", "QuickKey6", { }));
        //mQuickKey7 = std::move(createAction(XR_ACTION_TYPE_FLOAT_INPUT, "QuickKey7", "QuickKey7", { }));
        //mQuickKey8 = std::move(createAction(XR_ACTION_TYPE_FLOAT_INPUT, "QuickKey8", "QuickKey8", { }));
        //mQuickKey9 = std::move(createAction(XR_ACTION_TYPE_FLOAT_INPUT, "QuickKey9", "QuickKey9", { }));
        //mQuickKey10 = std::move(createAction(XR_ACTION_TYPE_FLOAT_INPUT, "QuickKey10", "QuickKey10", { }));
        //mQuickKeysMenu = std::move(createAction(XR_ACTION_TYPE_FLOAT_INPUT, "QuickKeysMenu", "QuickKeysMenu", { }));
        //mToggleHUD = std::move(createAction(XR_ACTION_TYPE_FLOAT_INPUT, "ToggleHUD", "ToggleHUD", { }));
        //mToggleDebug = std::move(createAction(XR_ACTION_TYPE_FLOAT_INPUT, "ToggleDebug", "ToggleDebug", { }));
        //mToggleSneak = std::move(createAction(XR_ACTION_TYPE_BOOLEAN_INPUT, "ToggleSneak", "ToggleSneak", { }));
        //mLookUpDown = std::move(createAction(XR_ACTION_TYPE_FLOAT_INPUT, "LookUpDown", "LookUpDown", { }));
        //mZoomIn = std::move(createAction(XR_ACTION_TYPE_FLOAT_INPUT, "ZoomIn", "ZoomIn", { }));
        //mZoomOut = std::move(createAction(XR_ACTION_TYPE_FLOAT_INPUT, "ZoomOut", "ZoomOut", { }));

        { // Set up default bindings for the oculus
            XrPath oculusTouchInteractionProfilePath;
            CHECK_XRCMD(
                xrStringToPath(XR->mPrivate->mInstance, "/interaction_profiles/oculus/touch_controller", &oculusTouchInteractionProfilePath));
            std::vector<XrActionSuggestedBinding> bindings{ {
                {mHandPoseAction, mPosePath[LEFT_HAND]},
                {mHandPoseAction, mPosePath[RIGHT_HAND]},
                {mHapticsAction, mHapticPath[LEFT_HAND]},
                {mHapticsAction, mHapticPath[RIGHT_HAND]},
                {mLookLeftRight, mThumbstickXPath[RIGHT_HAND]},
                {mMoveLeftRight, mThumbstickXPath[LEFT_HAND]},
                {mMoveForwardBackward, mThumbstickYPath[LEFT_HAND]},
                {mActivate, mSqueezeClickPath[RIGHT_HAND]},
                {mUse, mTriggerClickPath[RIGHT_HAND]},
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
                {mSpellModifier, mSqueezeClickPath[LEFT_HAND]},
                {mGameMenu, mMenuClickPath[LEFT_HAND]},
              } };
            XrInteractionProfileSuggestedBinding suggestedBindings{ XR_TYPE_INTERACTION_PROFILE_SUGGESTED_BINDING };
            suggestedBindings.interactionProfile = oculusTouchInteractionProfilePath;
            suggestedBindings.suggestedBindings = bindings.data();
            suggestedBindings.countSuggestedBindings = (uint32_t)bindings.size();
            CHECK_XRCMD(xrSuggestInteractionProfileBindings(XR->mPrivate->mInstance, &suggestedBindings));

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
            CHECK_XRCMD(xrCreateActionSpace(XR->mPrivate->mSession, &createInfo, &mHandSpace[LEFT_HAND]));
            createInfo.subactionPath = mSubactionPath[RIGHT_HAND];
            CHECK_XRCMD(xrCreateActionSpace(XR->mPrivate->mSession, &createInfo, &mHandSpace[RIGHT_HAND]));
        }

        { // Set up the action set
            XrSessionActionSetsAttachInfo attachInfo{ XR_TYPE_SESSION_ACTION_SETS_ATTACH_INFO };
            attachInfo.countActionSets = 1;
            attachInfo.actionSets = &mActionSet;
            CHECK_XRCMD(xrAttachSessionActionSets(XR->mPrivate->mSession, &attachInfo));
        }
    };

    OpenXRAction
        OpenXRInputManagerImpl::createAction(
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

    void
        OpenXRInputManagerImpl::updateHandTracking()
    {
        for (auto hand : { LEFT_HAND, RIGHT_HAND }) {
            CHECK_XRCMD(xrLocateSpace(mHandSpace[hand], mXR->mPrivate->mReferenceSpaceStage, mXR->mPrivate->mFrameState.predictedDisplayTime, &mHandSpaceLocation[hand]));
        }
    }

    XrPath 
        OpenXRInputManagerImpl::subactionPath(
            SubAction subAction)
    {
        if (subAction == NONE)
            return 0;
        return mSubactionPath[subAction];
    }

    void 
        OpenXRInputManagerImpl::updateControls()
    {
        if (!mXR->mPrivate->mSessionRunning)
            return;

        // TODO: Should be in OpenXRViewer
        //mXR->waitFrame();
        //mXR->beginFrame();

        updateHead();
        updateHands();

        // This set of actions only care about on-press
        if (mActionsMenu.actionOnPress())
        {
            // Generate on press action
        }
        if (mGameMenu.actionOnPress())
        {
            // Generate on press action
        }
        if (mInventory.actionOnPress())
        {
            // Generate on press action
        }
        if (mActivate.actionOnPress())
        {
            // Generate on press action
        }
        if (mUse.actionOnPress())
        {
            // Generate on press action
        }
        if (mJump.actionOnPress())
        {
            // Generate on press action
        }
        if (mSneak.actionOnPress())
        {
            // Generate on press action
        }
        if (mQuickMenu.actionOnPress())
        {
            // Generate on press action
        }

        // Weapon/Spell actions
        if (mWeapon.actionOnPress() && !mSpellModifier.actionIsPressed())
        {
            // Generate on press action
        }
        if (mCycleWeaponLeft.actionOnPress() && !mSpellModifier.actionIsPressed())
        {
            // Generate on press action
        }
        if (mCycleWeaponRight.actionOnPress() && !mSpellModifier.actionIsPressed())
        {
            // Generate on press action
        }
        if (mSpell.actionOnPress() && mSpellModifier.actionIsPressed())
        {
            // Generate on press action
        }
        if (mCycleSpellLeft.actionOnPress() && mSpellModifier.actionIsPressed())
        {
            // Generate on press action
        }
        if (mCycleSpellRight.actionOnPress() && mSpellModifier.actionIsPressed())
        {
            // Generate on press action
        }


        float lookLeftRight = mLookLeftRight.value();
        float moveLeftRight = mMoveLeftRight.value();
        float moveForwardBackward = mMoveForwardBackward.value();

        // Propagate movement to openmw
    }

    void OpenXRInputManagerImpl::updateHead()
    {
        //auto location = mXR->getHeadLocation();

        ////std::stringstream ss;
        ////ss << "Head.pose=< position=<"
        ////    << location.pose.position.x << ", " << location.pose.position.y << ", " << location.pose.position.z << "> orientation=<"
        ////    << location.pose.orientation.x << ", " << location.pose.orientation.y << ", " << location.pose.orientation.z << ", " << location.pose.orientation.w << "> >";
        ////mOpenXRLogger.log(ss.str(), 5, "Tracking", "OpenXR");

        //// To keep world movement from physical walking properly oriented, world position must be tracked in differentials from stage position, as orientation between
        //// stage and world may differ.
        //osg::Vec3f newHeadStagePosition{ location.pose.position.x, location.pose.position.y, location.pose.position.z };
        //osg::Vec3f headStageMovement = newHeadStagePosition - mHeadStagePosition;
        //osg::Vec3f headWorldMovement = yaw() * headStageMovement;

        //// Update positions
        //mHeadStagePosition = newHeadStagePosition;
        //mHeadWorldPosition = mHeadWorldPosition + headWorldMovement;

        //// Update orientations
        //mHeadStageOrientation = osg::fromXR(location.pose.orientation);
        //mHeadWorldOrientation = yaw() * mHeadStageOrientation;

        //osg::Vec3f up(0.f, 1.f, 0.f);
        //up = mHeadStageOrientation * up;

        //mHeadUpward = (mHeadWorldOrientation * osg::Vec3f(0.f, 1.f, 0.f));
        //mHeadUpward.normalize();
        //mHeadForward = (mHeadWorldOrientation * osg::Vec3f(0.f, 0.f, -1.f));
        //mHeadForwardmHeadForward.normalize();
        //mHeadRightward = mHeadForward ^ mHeadUpward;
        //mHeadRightward.normalize();
    }

    void OpenXRInputManagerImpl::updateHands()
    {
        // TODO
    }

    XrPath OpenXRInputManagerImpl::generateXrPath(const std::string& path)
    {
        XrPath xrpath = 0;
        CHECK_XRCMD(xrStringToPath(mXR->mPrivate->mInstance, path.c_str(), &xrpath));
        return xrpath;
    }

    OpenXRInputManager::OpenXRInputManager(
        osg::ref_ptr<OpenXRManager> XR)
        : mPrivate(new OpenXRInputManagerImpl(XR))
    {

    }

    OpenXRInputManager::~OpenXRInputManager()
    {

    }

    void 
        OpenXRInputManager::updateControls()
    {
        mPrivate->updateControls();
    }
}
