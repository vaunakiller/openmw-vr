#include "openxrinput.hpp"

#include <openxr/openxr.h>

#include <components/misc/stringops.hpp>
#include <components/vr/trackingmanager.hpp>
#include <components/xr/session.hpp>
#include <components/xr/instance.hpp>
#include <components/xr/debug.hpp>

#include <iostream>

#include <extern/oics/tinyxml.h>

namespace MWVR
{

    OpenXRInput::OpenXRInput(const std::string& xrControllerSuggestionsFile)
        : mXrControllerSuggestionsFile(xrControllerSuggestionsFile)
    {
        createActionSets();
        createGameplayActions();
        createGUIActions();
        createPoseActions();
        createHapticActions();
        readXrControllerSuggestions();
        attachActionSets();
    }

    void OpenXRInput::createActionSets()
    {
        mActionSets.emplace(std::piecewise_construct, std::forward_as_tuple(MWActionSet::Gameplay), std::forward_as_tuple("Gameplay", mDeadzone));
        mActionSets.emplace(std::piecewise_construct, std::forward_as_tuple(MWActionSet::GUI), std::forward_as_tuple("GUI", mDeadzone));
        mActionSets.emplace(std::piecewise_construct, std::forward_as_tuple(MWActionSet::Tracking), std::forward_as_tuple("Tracking", mDeadzone));
        mActionSets.emplace(std::piecewise_construct, std::forward_as_tuple(MWActionSet::Haptics), std::forward_as_tuple("Haptics", mDeadzone));
    }

    void OpenXRInput::createGameplayActions()
    {
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
            A_Screenshot, // Generate a VR screenshot? Currently not applicable because the screenshot function crashes the viewer.
            A_Console,    
        */
        getActionSet(MWActionSet::Gameplay).createMWAction(XR::ControlType::Press, MWInput::A_GameMenu, "game_menu", "Game Menu");
        getActionSet(MWActionSet::Gameplay).createMWAction(XR::ControlType::Press, A_VrMetaMenu, "meta_menu", "Meta Menu");
        getActionSet(MWActionSet::Gameplay).createMWAction(XR::ControlType::LongPress, A_Recenter, "reposition_menu", "Reposition Menu");
        getActionSet(MWActionSet::Gameplay).createMWAction(XR::ControlType::Press, MWInput::A_Inventory, "inventory", "Inventory");
        getActionSet(MWActionSet::Gameplay).createMWAction(XR::ControlType::Press, MWInput::A_Activate, "activate", "Activate");
        getActionSet(MWActionSet::Gameplay).createMWAction(XR::ControlType::Hold, MWInput::A_Use, "use", "Use");
        getActionSet(MWActionSet::Gameplay).createMWAction(XR::ControlType::Hold, MWInput::A_Jump, "jump", "Jump");
        getActionSet(MWActionSet::Gameplay).createMWAction(XR::ControlType::Press, MWInput::A_ToggleWeapon, "weapon", "Weapon");
        getActionSet(MWActionSet::Gameplay).createMWAction(XR::ControlType::Press, MWInput::A_ToggleSpell, "spell", "Spell");
        getActionSet(MWActionSet::Gameplay).createMWAction(XR::ControlType::Press, MWInput::A_CycleSpellLeft, "cycle_spell_left", "Cycle Spell Left");
        getActionSet(MWActionSet::Gameplay).createMWAction(XR::ControlType::Press, MWInput::A_CycleSpellRight, "cycle_spell_right", "Cycle Spell Right");
        getActionSet(MWActionSet::Gameplay).createMWAction(XR::ControlType::Press, MWInput::A_CycleWeaponLeft, "cycle_weapon_left", "Cycle Weapon Left");
        getActionSet(MWActionSet::Gameplay).createMWAction(XR::ControlType::Press, MWInput::A_CycleWeaponRight, "cycle_weapon_right", "Cycle Weapon Right");
        getActionSet(MWActionSet::Gameplay).createMWAction(XR::ControlType::Hold, MWInput::A_Sneak, "sneak", "Sneak");
        getActionSet(MWActionSet::Gameplay).createMWAction(XR::ControlType::Press, MWInput::A_QuickKeysMenu, "quick_menu", "Quick Menu");
        getActionSet(MWActionSet::Gameplay).createMWAction(XR::ControlType::Axis, MWInput::A_LookLeftRight, "look_left_right", "Look Left Right");
        getActionSet(MWActionSet::Gameplay).createMWAction(XR::ControlType::Axis, MWInput::A_MoveForwardBackward, "move_forward_backward", "Move Forward Backward");
        getActionSet(MWActionSet::Gameplay).createMWAction(XR::ControlType::Axis, MWInput::A_MoveLeftRight, "move_left_right", "Move Left Right");
        getActionSet(MWActionSet::Gameplay).createMWAction(XR::ControlType::Press, MWInput::A_Journal, "journal_book", "Journal Book");
        getActionSet(MWActionSet::Gameplay).createMWAction(XR::ControlType::Press, MWInput::A_QuickSave, "quick_save", "Quick Save");
        getActionSet(MWActionSet::Gameplay).createMWAction(XR::ControlType::Press, MWInput::A_Rest, "rest", "Rest");
        getActionSet(MWActionSet::Gameplay).createMWAction(XR::ControlType::Axis, A_ActivateTouch, "activate_touched", "Activate Touch");
        getActionSet(MWActionSet::Gameplay).createMWAction(XR::ControlType::Press, MWInput::A_AlwaysRun, "always_run", "Always Run");
        getActionSet(MWActionSet::Gameplay).createMWAction(XR::ControlType::Press, MWInput::A_AutoMove, "auto_move", "Auto Move");
        getActionSet(MWActionSet::Gameplay).createMWAction(XR::ControlType::Press, MWInput::A_ToggleHUD, "toggle_hud", "Toggle HUD");
        getActionSet(MWActionSet::Gameplay).createMWAction(XR::ControlType::Press, MWInput::A_ToggleDebug, "toggle_debug", "Toggle the debug hud");
    }

    void OpenXRInput::createGUIActions()
    {
        getActionSet(MWActionSet::GUI).createMWAction(XR::ControlType::Press, MWInput::A_GameMenu, "game_menu", "Game Menu");
        getActionSet(MWActionSet::GUI).createMWAction(XR::ControlType::LongPress, A_Recenter, "reposition_menu", "Reposition Menu");
        getActionSet(MWActionSet::GUI).createMWAction(XR::ControlType::Axis, A_MenuUpDown, "menu_up_down", "Menu Up Down");
        getActionSet(MWActionSet::GUI).createMWAction(XR::ControlType::Axis, A_MenuLeftRight, "menu_left_right", "Menu Left Right");
        getActionSet(MWActionSet::GUI).createMWAction(XR::ControlType::Press, A_MenuSelect, "menu_select", "Menu Select");
        getActionSet(MWActionSet::GUI).createMWAction(XR::ControlType::Press, A_MenuBack, "menu_back", "Menu Back");
        getActionSet(MWActionSet::GUI).createMWAction(XR::ControlType::Hold, MWInput::A_Use, "use", "Use");
    }

    void OpenXRInput::createPoseActions()
    {
        getActionSet(MWActionSet::Tracking).createPoseAction(VR::Side_Left, "left_hand_pose", "Left Hand Pose");
        getActionSet(MWActionSet::Tracking).createPoseAction(VR::Side_Right, "right_hand_pose", "Right Hand Pose");

        auto stageUserHandLeftPath = VR::stringToVRPath("/stage/user/hand/left/input/aim/pose");
        auto stageUserHandRightPath = VR::stringToVRPath("/stage/user/hand/right/input/aim/pose");
        auto worldUserHandLeftPath = VR::stringToVRPath("/world/user/hand/left/input/aim/pose");
        auto worldUserHandRightPath = VR::stringToVRPath("/world/user/hand/right/input/aim/pose");

        XR::Session::instance().tracker().setTrackingActionSet(&getActionSet(MWActionSet::Tracking));

        XR::Session::instance().tracker().addTrackingSpace(stageUserHandLeftPath, getActionSet(MWActionSet::Tracking).xrActionSpace(VR::Side_Left));
        XR::Session::instance().tracker().addTrackingSpace(stageUserHandRightPath, getActionSet(MWActionSet::Tracking).xrActionSpace(VR::Side_Right));
        XR::Session::instance().stageToWorldBinding().bindPaths(worldUserHandLeftPath, stageUserHandLeftPath);
        XR::Session::instance().stageToWorldBinding().bindPaths(worldUserHandRightPath, stageUserHandRightPath);
    }

    void OpenXRInput::createHapticActions()
    {
        getActionSet(MWActionSet::Haptics).createHapticsAction(VR::Side_Left, "left_hand_haptics", "Left Hand Haptics");
        getActionSet(MWActionSet::Haptics).createHapticsAction(VR::Side_Right, "right_hand_haptics", "Right Hand Haptics");
    }

    void OpenXRInput::readXrControllerSuggestions()
    {
        if (mXrControllerSuggestionsFile.empty())
            throw std::runtime_error("No interaction profiles available (xrcontrollersuggestions.xml not found)");

        Log(Debug::Verbose) << "Reading Input Profile Path suggestions from " << mXrControllerSuggestionsFile;

        TiXmlDocument* xmlDoc = nullptr;
        TiXmlElement* xmlRoot = nullptr;

        xmlDoc = new TiXmlDocument(mXrControllerSuggestionsFile.c_str());
        xmlDoc->LoadFile();

        if (xmlDoc->Error())
        {
            std::ostringstream message;
            message << "TinyXml reported an error reading \"" + mXrControllerSuggestionsFile + "\". Row " <<
                (int)xmlDoc->ErrorRow() << ", Col " << (int)xmlDoc->ErrorCol() << ": " <<
                xmlDoc->ErrorDesc();
            Log(Debug::Error) << message.str();
            throw std::runtime_error(message.str());

            delete xmlDoc;
            return;
        }

        xmlRoot = xmlDoc->RootElement();
        if (std::string(xmlRoot->Value()) != "Root") {
            Log(Debug::Verbose) << "Error: Invalid xr controllers file. Missing <Root> element.";
            delete xmlDoc;
            return;
        }

        TiXmlElement* profile = xmlRoot->FirstChildElement("Profile");
        while (profile)
        {
            readInteractionProfile(profile);
            profile = profile->NextSiblingElement("Profile");
        }
    }

    XR::ActionSet& OpenXRInput::getActionSet(MWActionSet actionSet)
    {
        auto it = mActionSets.find(actionSet);
        if (it == mActionSets.end())
            throw std::logic_error("No such action set");
        return it->second;
    }

    void OpenXRInput::suggestBindings(MWActionSet actionSet, std::string profilePath, const XR::SuggestedBindings& mwSuggestedBindings)
    {
        getActionSet(actionSet).suggestBindings(mSuggestedBindings[profilePath], mwSuggestedBindings);
    }

    void OpenXRInput::attachActionSets()
    {
        // Suggest bindings before attaching
        for (auto& profile : mSuggestedBindings)
        {
            XrPath profilePath = 0;
            CHECK_XRCMD(
                xrStringToPath(XR::Instance::instance().xrInstance(), profile.first.c_str(), &profilePath));
            XrInteractionProfileSuggestedBinding xrProfileSuggestedBindings{};
            xrProfileSuggestedBindings.type = XR_TYPE_INTERACTION_PROFILE_SUGGESTED_BINDING;
            xrProfileSuggestedBindings.interactionProfile = profilePath;
            xrProfileSuggestedBindings.suggestedBindings = profile.second.data();
            xrProfileSuggestedBindings.countSuggestedBindings = (uint32_t)profile.second.size();
            CHECK_XRCMD(xrSuggestInteractionProfileBindings(XR::Instance::instance().xrInstance(), &xrProfileSuggestedBindings));
            mInteractionProfileNames[profilePath] = profile.first;
            mInteractionProfilePaths[profile.first] = profilePath;
        }

        // OpenXR requires that xrAttachSessionActionSets be called at most once per session.
        // So collect all action sets
        std::vector<XrActionSet> actionSets;
        for (auto& actionSet : mActionSets)
            actionSets.push_back(actionSet.second.xrActionSet());

        // Attach
        XrSessionActionSetsAttachInfo attachInfo{};
        attachInfo.type = XR_TYPE_SESSION_ACTION_SETS_ATTACH_INFO;
        attachInfo.countActionSets = actionSets.size();
        attachInfo.actionSets = actionSets.data();
        CHECK_XRCMD(xrAttachSessionActionSets(XR::Session::instance().xrSession(), &attachInfo));
    }

    void OpenXRInput::notifyInteractionProfileChanged()
    {
        // Unfortunately, openxr does not tell us WHICH profile has changed.
        std::array<std::string, 5> topLevelUserPaths =
        {
            "/user/hand/left",
            "/user/hand/right",
            "/user/head",
            "/user/gamepad",
            "/user/treadmill"
        };

        for (auto& userPath : topLevelUserPaths)
        {
            auto pathIt = mInteractionProfilePaths.find(userPath);
            if (pathIt == mInteractionProfilePaths.end())
            {
                XrPath xrUserPath = XR_NULL_PATH;
                CHECK_XRCMD(
                    xrStringToPath(XR::Instance::instance().xrInstance(), userPath.c_str(), &xrUserPath));
                mInteractionProfilePaths[userPath] = xrUserPath;
                pathIt = mInteractionProfilePaths.find(userPath);
            }

            XrInteractionProfileState interactionProfileState{};
            interactionProfileState.type = XR_TYPE_INTERACTION_PROFILE_STATE;

            xrGetCurrentInteractionProfile(XR::Session::instance().xrSession(), pathIt->second, &interactionProfileState);
            if (interactionProfileState.interactionProfile)
            {
                auto activeProfileIt = mActiveInteractionProfiles.find(pathIt->second);
                if (activeProfileIt == mActiveInteractionProfiles.end() || interactionProfileState.interactionProfile != activeProfileIt->second)
                {
                    auto activeProfileNameIt = mInteractionProfileNames.find(interactionProfileState.interactionProfile);
                    Log(Debug::Verbose) << userPath << ": Interaction profile changed to '" << activeProfileNameIt->second << "'";
                    mActiveInteractionProfiles[pathIt->second] = interactionProfileState.interactionProfile;
                }
            }
        }
    }


    void OpenXRInput::throwDocumentError(TiXmlElement* element, std::string error)
    {
        std::stringstream ss;
        ss << mXrControllerSuggestionsFile << "." << element->Row() << "." << element->Value();
        ss << ": " << error;
        throw std::runtime_error(ss.str());
    }

    std::string OpenXRInput::requireAttribute(TiXmlElement* element, std::string attribute)
    {
        const char* value = element->Attribute(attribute.c_str());
        if (!value)
            throwDocumentError(element, std::string() + "Missing attribute '" + attribute + "'");
        return value;
    }

    void OpenXRInput::readInteractionProfile(TiXmlElement* element)
    {
        std::string interactionProfilePath = requireAttribute(element, "Path");
        mInteractionProfileLocalNames[interactionProfilePath] = requireAttribute(element, "LocalName");

        Log(Debug::Verbose) << "Configuring interaction profile '" << interactionProfilePath << "' (" << mInteractionProfileLocalNames[interactionProfilePath] << ")";

        // Check extension if present
        TiXmlElement* extensionElement = element->FirstChildElement("Extension");
        if (extensionElement)
        {
            std::string extension = requireAttribute(extensionElement, "Name");
            if (!XR::Extensions::instance().extensionEnabled(extension))
            {
                Log(Debug::Verbose) << "  Required extension '" << extension << "' not supported. Skipping interaction profile.";
                return;
            }
        }

        TiXmlElement* actionSetGameplay = nullptr;
        TiXmlElement* actionSetGUI = nullptr;
        TiXmlElement* child = element->FirstChildElement("ActionSet");
        while (child)
        {
            std::string name = requireAttribute(child, "Name");
            if (name == "Gameplay")
                actionSetGameplay = child;
            else if (name == "GUI")
                actionSetGUI = child;

            child = child->NextSiblingElement("ActionSet");
        }

        if (!actionSetGameplay)
            throwDocumentError(element, "Gameplay action set missing");
        if (!actionSetGUI)
            throwDocumentError(element, "GUI action set missing");

        readInteractionProfileActionSet(actionSetGameplay, MWActionSet::Gameplay, interactionProfilePath);
        readInteractionProfileActionSet(actionSetGUI, MWActionSet::GUI, interactionProfilePath);
        suggestBindings(MWActionSet::Tracking, interactionProfilePath, {});
        suggestBindings(MWActionSet::Haptics, interactionProfilePath, {});
    }

    void OpenXRInput::readInteractionProfileActionSet(TiXmlElement* element, MWActionSet actionSet, std::string interactionProfilePath)
    {
        XR::SuggestedBindings suggestedBindings;

        TiXmlElement* child = element->FirstChildElement("Binding");
        while (child)
        {
            std::string action = requireAttribute(child, "ActionName");
            std::string path = requireAttribute(child, "Path");

            suggestedBindings.push_back(
                XR::SuggestedBinding{
                    path, action
                });

            Log(Debug::Debug) << "  " << action << ": " << path;

            child = child->NextSiblingElement("Binding");
        }

        suggestBindings(actionSet, interactionProfilePath, suggestedBindings);
    }

    void OpenXRInput::setThumbstickDeadzone(float deadzoneRadius)
    {
        mDeadzone->setDeadzoneRadius(deadzoneRadius);
    }
}
