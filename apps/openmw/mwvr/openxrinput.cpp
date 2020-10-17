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

    void OpenXRInput::suggestBindings(ActionSet actionSet, std::string profile, const SuggestedBindings& mwSuggestedBindings)
    {
        getActionSet(actionSet).suggestBindings(mSuggestedBindings[profile], mwSuggestedBindings);
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
}
