#ifndef OPENXR_INPUT_HPP
#define OPENXR_INPUT_HPP

#include "vrinput.hpp"

#include <vector>
#include <array>

namespace MWVR
{

struct SuggestedBindings
{
    struct Binding
    {
        int         action;
        ActionPath  path;
        Side        side;
    };

    std::string controllerPath;
    std::vector<Binding> bindings;
};

class OpenXRInput
{
public:
    using Actions = MWInput::Actions;
    using ControllerActionPaths = std::array<XrPath, 2>;

    OpenXRInput(const std::vector<SuggestedBindings>& suggestedBindings);

    //! Update all controls and queue any actions
    void updateControls();

    //! Get next action from queue (repeat until null is returned)
    const Action* nextAction();

    //! Get current pose of limb in space.
    Pose getLimbPose(int64_t time, TrackedLimb limb);

    //! Apply haptics of the given intensity to the given limb
    void applyHaptics(TrackedLimb limb, float intensity);

protected:
    template<typename A, XrActionType AT = A::ActionType>
    void createMWAction(int openMWAction, const std::string& actionName, const std::string& localName);
    void createPoseAction(TrackedLimb limb, const std::string& actionName, const std::string& localName);
    void createHapticsAction(TrackedLimb limb, const std::string& actionName, const std::string& localName);
    std::unique_ptr<OpenXRAction> createXRAction(XrActionType actionType, const std::string& actionName, const std::string& localName);
    XrPath generateXrPath(const std::string& path);
    void generateControllerActionPaths(ActionPath actionPath, const std::string& controllerAction);
    XrActionSet createActionSet(void);
    void suggestBindings(const SuggestedBindings& suggestedBindings);
    XrPath getXrPath(ActionPath actionPath, Side side);

    XrActionSet mActionSet{ nullptr };

    std::map<ActionPath, ControllerActionPaths> mPathMap;
    std::map<int, std::unique_ptr<Action>> mActionMap;
    std::map<TrackedLimb, std::unique_ptr<PoseAction>> mTrackerMap;
    std::map<TrackedLimb, std::unique_ptr<HapticsAction>> mHapticsMap;

    std::deque<const Action*> mActionQueue{};
};
}

#endif
