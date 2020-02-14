#ifndef OPENXR_INPUT_MANAGER_HPP
#define OPENXR_INPUT_MANAGER_HPP

#include "openxrmanager.hpp"
#include "../mwinput/inputmanagerimp.hpp"

#include <vector>
#include <array>
#include <iostream>

namespace MWVR
{
    struct OpenXRActionEvent
    {
        MWInput::InputManager::Actions action;
        bool onPress;
    };

    struct OpenXRInputManagerImpl;
    struct OpenXRInputManager
    {
        OpenXRInputManager(osg::ref_ptr<OpenXRManager> XR);
        ~OpenXRInputManager();

        void updateControls();

        PoseSet getHandPoses(int64_t time, TrackedSpace space);

        bool nextActionEvent(OpenXRActionEvent& action);

        OpenXRInputManagerImpl& impl() { return *mPrivate; }

        std::unique_ptr<OpenXRInputManagerImpl> mPrivate;
    };
}

#endif
