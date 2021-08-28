#ifndef OPENXR_INPUT_HPP
#define OPENXR_INPUT_HPP

#include "vrinput.hpp"
#include "openxractionset.hpp"

#include <vector>
#include <array>

class TiXmlElement;

namespace MWVR
{
    /// Extension of MWInput's set of actions.
    enum VrActions
    {
        A_VrFirst = MWInput::A_Last + 1,
        A_VrMetaMenu,
        A_ActivateTouch,
        A_HapticsLeft,
        A_HapticsRight,
        A_HandPoseLeft,
        A_HandPoseRight,
        A_MenuUpDown,
        A_MenuLeftRight,
        A_MenuSelect,
        A_MenuBack,
        A_Recenter,
        A_VrLast
    };

    /// \brief Enumeration of action sets
    enum class ActionSet
    {
        GUI = 0,
        Gameplay = 1,
        Tracking = 2,
        Haptics = 3,
    };

    /// \brief Generates and manages OpenXR Actions and ActionSets by generating openxr bindings from a list of SuggestedBindings structs.
    class OpenXRInput
    {
    public:
        using XrSuggestedBindings = std::vector<XrActionSuggestedBinding>;
        using XrProfileSuggestedBindings = std::map<std::string, XrSuggestedBindings>;

        //! Default constructor, creates two ActionSets: Gameplay and GUI
        OpenXRInput(std::shared_ptr<AxisAction::Deadzone> deadzone, const std::string& xrControllerSuggestionsFile);
        void createActionSets();
        void createGameplayActions();
        void createGUIActions();
        void createPoseActions();
        void createHapticActions();

        void readXrControllerSuggestions();

        //! Get the specified actionSet.
        OpenXRActionSet& getActionSet(ActionSet actionSet);

        //! Suggest bindings for the specific actionSet and profile pair. Call things after calling attachActionSets is an error.
        void suggestBindings(ActionSet actionSet, std::string profile, const SuggestedBindings& mwSuggestedBindings);

        //! Set bindings and attach actionSets to the session.
        void attachActionSets();

        //! Notify that active interaction profile has changed
        void notifyInteractionProfileChanged();

        void throwDocumentError(TiXmlElement* element, std::string error);
        std::string requireAttribute(TiXmlElement* element, std::string attribute);
        void readInteractionProfile(TiXmlElement* element);
        void readInteractionProfileActionSet(TiXmlElement* element, ActionSet actionSet, std::string profilePath);

    protected:
        std::shared_ptr<AxisAction::Deadzone> mDeadzone;
        std::string mXrControllerSuggestionsFile;
        std::map<std::string, std::string> mInteractionProfileLocalNames{};
        std::map<ActionSet, OpenXRActionSet> mActionSets{};
        std::map<XrPath, std::string> mInteractionProfileNames{};
        std::map<std::string, XrPath> mInteractionProfilePaths{};
        std::map<XrPath, XrPath> mActiveInteractionProfiles;
        XrProfileSuggestedBindings mSuggestedBindings{};
        bool mAttached = false;
    };
}

#endif
