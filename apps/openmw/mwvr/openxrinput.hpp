#ifndef OPENXR_INPUT_HPP
#define OPENXR_INPUT_HPP

#include "vrinput.hpp"
#include "openxractionset.hpp"

#include <vector>
#include <array>

namespace MWVR
{
    /// \brief Generates and manages OpenXR Actions and ActionSets by generating openxr bindings from a list of SuggestedBindings structs.
    class OpenXRInput
    {
    public:
        using XrSuggestedBindings = std::vector<XrActionSuggestedBinding>;
        using XrProfileSuggestedBindings = std::map<std::string, XrSuggestedBindings>;

        //! Default constructor, creates two ActionSets: Gameplay and GUI
        OpenXRInput();

        //! Get the specified actionSet.
        OpenXRActionSet& getActionSet(ActionSet actionSet);

        //! Suggest bindings for the specific actionSet and profile pair. Call things after calling attachActionSets is an error.
        void suggestBindings(ActionSet actionSet, std::string profile, const SuggestedBindings& mwSuggestedBindings);

        //! Set bindings and attach actionSets to the session.
        void attachActionSets();

    protected:
        std::map<ActionSet, OpenXRActionSet> mActionSets{};
        XrProfileSuggestedBindings mSuggestedBindings{};
        bool mAttached = false;
    };
}

#endif
