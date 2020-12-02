#include "openxrinput.hpp"

#include "vrenvironment.hpp"
#include "openxrmanager.hpp"
#include "openxrmanagerimpl.hpp"
#include "openxraction.hpp"

#include <openxr/openxr.h>

#include <components/misc/stringops.hpp>

#include <iostream>

namespace MWVR
{

    OpenXRInput::OpenXRInput()
    {
        mActionSets.emplace(ActionSet::Gameplay, "Gameplay");
        mActionSets.emplace(ActionSet::GUI, "GUI");
    };

    OpenXRActionSet& OpenXRInput::getActionSet(ActionSet actionSet)
    {
        auto it = mActionSets.find(actionSet);
        if (it == mActionSets.end())
            throw std::logic_error("No such action set");
        return it->second;
    }

    void OpenXRInput::suggestBindings(ActionSet actionSet, std::string profilePath, const SuggestedBindings& mwSuggestedBindings)
    {
        getActionSet(actionSet).suggestBindings(mSuggestedBindings[profilePath], mwSuggestedBindings);
    }

    void OpenXRInput::attachActionSets()
    {
        auto* xr = Environment::get().getManager();

        // Suggest bindings before attaching
        for (auto& profile : mSuggestedBindings)
        {
            XrPath profilePath = 0;
            CHECK_XRCMD(
                xrStringToPath(xr->impl().xrInstance(), profile.first.c_str(), &profilePath));
            XrInteractionProfileSuggestedBinding xrProfileSuggestedBindings{ XR_TYPE_INTERACTION_PROFILE_SUGGESTED_BINDING };
            xrProfileSuggestedBindings.interactionProfile = profilePath;
            xrProfileSuggestedBindings.suggestedBindings = profile.second.data();
            xrProfileSuggestedBindings.countSuggestedBindings = (uint32_t)profile.second.size();
            CHECK_XRCMD(xrSuggestInteractionProfileBindings(xr->impl().xrInstance(), &xrProfileSuggestedBindings));
            mInteractionProfileNames[profilePath] = profile.first;
            mInteractionProfilePaths[profile.first] = profilePath;
        }

        // OpenXR requires that xrAttachSessionActionSets be called at most once per session.
        // So collect all action sets
        std::vector<XrActionSet> actionSets;
        for (auto& actionSet : mActionSets)
            actionSets.push_back(actionSet.second.xrActionSet());

        // Attach
        XrSessionActionSetsAttachInfo attachInfo{ XR_TYPE_SESSION_ACTION_SETS_ATTACH_INFO };
        attachInfo.countActionSets = actionSets.size();
        attachInfo.actionSets = actionSets.data();
        CHECK_XRCMD(xrAttachSessionActionSets(xr->impl().xrSession(), &attachInfo));
    }

    void OpenXRInput::notifyInteractionProfileChanged()
    {
        auto xr = MWVR::Environment::get().getManager();
        xr->impl().xrSession();

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
                    xrStringToPath(xr->impl().xrInstance(), userPath.c_str(), &xrUserPath));
                mInteractionProfilePaths[userPath] = xrUserPath;
                pathIt = mInteractionProfilePaths.find(userPath);
            }

            XrInteractionProfileState interactionProfileState{
                XR_TYPE_INTERACTION_PROFILE_STATE
            };

            xrGetCurrentInteractionProfile(xr->impl().xrSession(), pathIt->second, &interactionProfileState);
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
}
