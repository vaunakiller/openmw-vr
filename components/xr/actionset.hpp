#ifndef XR_ACTIONSET_HPP
#define XR_ACTIONSET_HPP

#include <components/vr/constants.hpp>
#include "action.hpp"

#include <vector>
#include <array>
#include <memory>
#include <map>

namespace XR
{

    /// \brief Suggest a binding by binding an action to a path on a given hand (left or right).
    struct SuggestedBinding
    {
        std::string path;
        std::string action;
    };

    using SuggestedBindings = std::vector<SuggestedBinding>;

    /// \brief Generates and manages an OpenXR ActionSet and associated actions.
    class ActionSet
    {
    public:
        ActionSet(const std::string& actionSetName, std::shared_ptr<AxisAction::Deadzone> deadzone);

        //! Update all controls and queue any actions
        void updateControls();

        //! Get next action from queue (repeat until null is returned)
        const InputAction* nextAction();

        //! Apply haptics of the given intensity to the given limb
        void applyHaptics(VR::Side side, float intensity);

        XrActionSet xrActionSet() { return mActionSet; };
        void suggestBindings(std::vector<XrActionSuggestedBinding>& xrSuggestedBindings, const SuggestedBindings& mwSuggestedBindings);

        XrSpace xrActionSpace(VR::Side side);

        void createMWAction(ControlType controlType, int openMWAction, const std::string& actionName, const std::string& localName);
        void createPoseAction(VR::Side side, const std::string& actionName, const std::string& localName);
        void createHapticsAction(VR::Side side, const std::string& actionName, const std::string& localName);

    protected:
        template<typename A>
        void createMWAction(int openMWAction, const std::string& actionName, const std::string& localName);
        std::unique_ptr<XR::Action> createXRAction(XrActionType actionType, const std::string& actionName, const std::string& localName);
        XrPath getXrPath(const std::string& path);
        XrActionSet createActionSet(const std::string& name);

        XrActionSet mActionSet{ nullptr };
        std::string mLocalizedName{};
        std::string mInternalName{};
        std::map<std::string, std::unique_ptr<InputAction>> mActionMap;
        std::map<VR::Side, std::unique_ptr<PoseAction>> mTrackerMap;
        std::map<VR::Side, std::unique_ptr<HapticsAction>> mHapticsMap;
        std::deque<const InputAction*> mActionQueue{};
        std::shared_ptr<AxisAction::Deadzone> mDeadzone;
    };
}

#endif
