#include "openxractionset.hpp"

#include "vrenvironment.hpp"
#include "openxrmanager.hpp"
#include "openxrmanagerimpl.hpp"
#include "openxraction.hpp"

#include <openxr/openxr.h>

#include <components/misc/stringops.hpp>

#include <iostream>

namespace MWVR
{

    OpenXRActionSet::OpenXRActionSet(const std::string& actionSetName)
        : mActionSet(nullptr)
        , mLocalizedName(actionSetName)
        , mInternalName(Misc::StringUtils::lowerCase(actionSetName))
    {
        mActionSet = createActionSet(actionSetName);
        // When starting to account for more devices than oculus touch, this section may need some expansion/redesign.

        // Currently the set of action paths was determined using the oculus touch (i know nothing about the vive and the index).
        // The set of action paths may therefore need expansion. E.g. /click vs /value may vary with controllers.

        // To fit more actions onto controllers i created a system of short and long press actions. Allowing one action to activate
        // on a short press, and another on long. Here, what actions are short press and what actions are long press is simply
        // hardcoded at init, rather than interpreted from bindings. That's bad, and should be fixed, but that's hard to do
        // while staying true to openxr's binding system, so if the system i wrote for the oculus touch isn't a good fit for
        // the vive/index, we might want to rewrite this to handle bindings ourselves.
        generateControllerActionPaths(ActionPath::Select, "/input/select/click");
        generateControllerActionPaths(ActionPath::Squeeze, "/input/squeeze/value");
        generateControllerActionPaths(ActionPath::Pose, "/input/aim/pose");
        generateControllerActionPaths(ActionPath::Haptic, "/output/haptic");
        generateControllerActionPaths(ActionPath::Menu, "/input/menu/click");
        generateControllerActionPaths(ActionPath::ThumbstickX, "/input/thumbstick/x");
        generateControllerActionPaths(ActionPath::ThumbstickY, "/input/thumbstick/y");
        generateControllerActionPaths(ActionPath::ThumbstickClick, "/input/thumbstick/click");
        generateControllerActionPaths(ActionPath::X, "/input/x/click");
        generateControllerActionPaths(ActionPath::Y, "/input/y/click");
        generateControllerActionPaths(ActionPath::A, "/input/a/click");
        generateControllerActionPaths(ActionPath::B, "/input/b/click");
        generateControllerActionPaths(ActionPath::Trigger, "/input/trigger/value");

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
        createMWAction<ButtonPressAction>(MWInput::A_GameMenu, "game_menu", "Game Menu");
        createMWAction<ButtonLongPressAction>(A_Recenter, "reposition_menu", "Reposition Menu");
        createMWAction<ButtonPressAction>(MWInput::A_Inventory, "inventory", "Inventory");
        createMWAction<ButtonPressAction>(MWInput::A_Activate, "activate", "Activate");
        createMWAction<ButtonHoldAction>(MWInput::A_Use, "use", "Use");
        createMWAction<ButtonHoldAction>(MWInput::A_Jump, "jump", "Jump");
        createMWAction<ButtonPressAction>(MWInput::A_ToggleWeapon, "weapon", "Weapon");
        createMWAction<ButtonPressAction>(MWInput::A_ToggleSpell, "spell", "Spell");
        createMWAction<ButtonPressAction>(MWInput::A_CycleSpellLeft, "cycle_spell_left", "Cycle Spell Left");
        createMWAction<ButtonPressAction>(MWInput::A_CycleSpellRight, "cycle_spell_right", "Cycle Spell Right");
        createMWAction<ButtonPressAction>(MWInput::A_CycleWeaponLeft, "cycle_weapon_left", "Cycle Weapon Left");
        createMWAction<ButtonPressAction>(MWInput::A_CycleWeaponRight, "cycle_weapon_right", "Cycle Weapon Right");
        createMWAction<ButtonHoldAction>(MWInput::A_Sneak, "sneak", "Sneak");
        createMWAction<ButtonPressAction>(MWInput::A_QuickMenu, "quick_menu", "Quick Menu");
        createMWAction<AxisAction>(MWInput::A_LookLeftRight, "look_left_right", "Look Left Right");
        createMWAction<AxisAction>(MWInput::A_MoveForwardBackward, "move_forward_backward", "Move Forward Backward");
        createMWAction<AxisAction>(MWInput::A_MoveLeftRight, "move_left_right", "Move Left Right");
        createMWAction<ButtonLongPressAction>(MWInput::A_Journal, "journal_book", "Journal Book");
        createMWAction<ButtonLongPressAction>(MWInput::A_QuickSave, "quick_save", "Quick Save");
        createMWAction<ButtonPressAction>(MWInput::A_Rest, "rest", "Rest");
        createMWAction<AxisAction>(A_ActivateTouch, "activate_touched", "Activate Touch");
        createMWAction<ButtonPressAction>(MWInput::A_AlwaysRun, "always_run", "Always Run");
        createMWAction<ButtonPressAction>(MWInput::A_AutoMove, "auto_move", "Auto Move");
        createMWAction<ButtonLongPressAction>(MWInput::A_ToggleHUD, "toggle_hud", "Toggle HUD");
        createMWAction<ButtonLongPressAction>(MWInput::A_ToggleDebug, "toggle_debug", "Toggle DEBUG");
        createMWAction<AxisAction>(A_MenuUpDown, "menu_up_down", "Menu Up Down");
        createMWAction<AxisAction>(A_MenuLeftRight, "menu_left_right", "Menu Left Right");
        createMWAction<ButtonPressAction>(A_MenuSelect, "menu_select", "Menu Select");
        createMWAction<ButtonPressAction>(A_MenuBack, "menu_back", "Menu Back");
        createPoseAction(TrackedLimb::LEFT_HAND, "left_hand_pose", "Left Hand Pose");
        createPoseAction(TrackedLimb::RIGHT_HAND, "right_hand_pose", "Right Hand Pose");
        createHapticsAction(TrackedLimb::RIGHT_HAND, "right_hand_haptics", "Right Hand Haptics");
        createHapticsAction(TrackedLimb::LEFT_HAND, "left_hand_haptics", "Left Hand Haptics");
    };

    void
        OpenXRActionSet::createPoseAction(
            TrackedLimb limb,
            const std::string& actionName,
            const std::string& localName)
    {
        mTrackerMap.emplace(limb, new PoseAction(std::move(createXRAction(XR_ACTION_TYPE_POSE_INPUT, actionName, localName))));
    }

    void
        OpenXRActionSet::createHapticsAction(
            TrackedLimb limb,
            const std::string& actionName,
            const std::string& localName)
    {
        mHapticsMap.emplace(limb, new HapticsAction(std::move(createXRAction(XR_ACTION_TYPE_VIBRATION_OUTPUT, actionName, localName))));
    }

    template<typename A, XrActionType AT>
    void
        OpenXRActionSet::createMWAction(
            int openMWAction,
            const std::string& actionName,
            const std::string& localName)
    {
        mActionMap.emplace(openMWAction, new A(openMWAction, std::move(createXRAction(AT, mInternalName + "_" + actionName, mLocalizedName + " " + localName))));
    }

    XrActionSet
        OpenXRActionSet::createActionSet(const std::string& name)
    {
        std::string localized_name = name;
        std::string internal_name = Misc::StringUtils::lowerCase(name);
        auto* xr = Environment::get().getManager();
        XrActionSet actionSet = XR_NULL_HANDLE;
        XrActionSetCreateInfo createInfo{ XR_TYPE_ACTION_SET_CREATE_INFO };
        strcpy_s(createInfo.actionSetName, internal_name.c_str());
        strcpy_s(createInfo.localizedActionSetName, localized_name.c_str());
        createInfo.priority = 0;
        CHECK_XRCMD(xrCreateActionSet(xr->impl().xrInstance(), &createInfo, &actionSet));
        return actionSet;
    }

    void OpenXRActionSet::suggestBindings(std::vector<XrActionSuggestedBinding>& xrSuggestedBindings, const SuggestedBindings& mwSuggestedBindings)
    {
        std::vector<XrActionSuggestedBinding> suggestedBindings =
        {
            {*mTrackerMap[TrackedLimb::LEFT_HAND], getXrPath(ActionPath::Pose, Side::LEFT_SIDE)},
            {*mTrackerMap[TrackedLimb::RIGHT_HAND], getXrPath(ActionPath::Pose, Side::RIGHT_SIDE)},
            {*mHapticsMap[TrackedLimb::LEFT_HAND], getXrPath(ActionPath::Haptic, Side::LEFT_SIDE)},
            {*mHapticsMap[TrackedLimb::RIGHT_HAND], getXrPath(ActionPath::Haptic, Side::RIGHT_SIDE)},
        };

        for (auto& mwSuggestedBinding : mwSuggestedBindings)
        {
            auto xrAction = mActionMap.find(mwSuggestedBinding.action);
            if (xrAction == mActionMap.end())
            {
                Log(Debug::Error) << "OpenXRActionSet: Unknown action " << mwSuggestedBinding.action;
                continue;
            }
            suggestedBindings.push_back({ *xrAction->second, getXrPath(mwSuggestedBinding.path, mwSuggestedBinding.side) });
        }

        xrSuggestedBindings.insert(xrSuggestedBindings.end(), suggestedBindings.begin(), suggestedBindings.end());
    }

    void
        OpenXRActionSet::generateControllerActionPaths(
            ActionPath actionPath,
            const std::string& controllerAction)
    {
        auto* xr = Environment::get().getManager();
        ControllerActionPaths actionPaths;

        std::string left = std::string("/user/hand/left") + controllerAction;
        std::string right = std::string("/user/hand/right") + controllerAction;

        CHECK_XRCMD(xrStringToPath(xr->impl().xrInstance(), left.c_str(), &actionPaths[(int)Side::LEFT_SIDE]));
        CHECK_XRCMD(xrStringToPath(xr->impl().xrInstance(), right.c_str(), &actionPaths[(int)Side::RIGHT_SIDE]));

        mPathMap[actionPath] = actionPaths;
    }


    std::unique_ptr<OpenXRAction>
        OpenXRActionSet::createXRAction(
            XrActionType actionType,
            const std::string& actionName,
            const std::string& localName)
    {
        std::vector<XrPath> subactionPaths;
        XrActionCreateInfo createInfo{ XR_TYPE_ACTION_CREATE_INFO };
        createInfo.actionType = actionType;
        strcpy_s(createInfo.actionName, actionName.c_str());
        strcpy_s(createInfo.localizedActionName, localName.c_str());

        XrAction action = XR_NULL_HANDLE;
        CHECK_XRCMD(xrCreateAction(mActionSet, &createInfo, &action));
        return std::unique_ptr<OpenXRAction>{new OpenXRAction{ action, actionType, actionName, localName }};
    }

    void
        OpenXRActionSet::updateControls()
    {
        auto* xr = Environment::get().getManager();
        if (!xr->impl().xrSessionRunning())
            return;

        const XrActiveActionSet activeActionSet{ mActionSet, XR_NULL_PATH };
        XrActionsSyncInfo syncInfo{ XR_TYPE_ACTIONS_SYNC_INFO };
        syncInfo.countActiveActionSets = 1;
        syncInfo.activeActionSets = &activeActionSet;
        CHECK_XRCMD(xrSyncActions(xr->impl().xrSession(), &syncInfo));

        for (auto& action : mActionMap)
            action.second->updateAndQueue(mActionQueue);
    }

    XrPath OpenXRActionSet::generateXrPath(const std::string& path)
    {
        auto* xr = Environment::get().getManager();
        XrPath xrpath = 0;
        CHECK_XRCMD(xrStringToPath(xr->impl().xrInstance(), path.c_str(), &xrpath));
        return xrpath;
    }

    const Action* OpenXRActionSet::nextAction()
    {
        if (mActionQueue.empty())
            return nullptr;

        const auto* action = mActionQueue.front();
        mActionQueue.pop_front();
        return action;

    }

    Pose
        OpenXRActionSet::getLimbPose(
            int64_t time,
            TrackedLimb limb)
    {
        auto it = mTrackerMap.find(limb);
        if (it == mTrackerMap.end())
        {
            Log(Debug::Error) << "OpenXRActionSet: No such tracker: " << limb;
            return Pose{};
        }

        it->second->update(time);
        return it->second->value();
    }

    void OpenXRActionSet::applyHaptics(TrackedLimb limb, float intensity)
    {
        auto it = mHapticsMap.find(limb);
        if (it == mHapticsMap.end())
        {
            Log(Debug::Error) << "OpenXRActionSet: No such tracker: " << limb;
            return;
        }

        it->second->apply(intensity);
    }
    XrPath OpenXRActionSet::getXrPath(ActionPath actionPath, Side side)
    {
        auto it = mPathMap.find(actionPath);
        if (it == mPathMap.end())
        {
            Log(Debug::Error) << "OpenXRActionSet: No such path: " << (int)actionPath;
        }
        return it->second[(int)side];
    }
}
