#ifndef OPENXR_INPUT_MANAGER_HPP
#define OPENXR_INPUT_MANAGER_HPP

#include "openxrmanager.hpp"
#include "../mwinput/inputmanagerimp.hpp"

#include <vector>
#include <array>
#include <iostream>

namespace MWVR
{
    struct OpenXRInputManagerImpl;
    struct OpenXRInputManager
    {
        OpenXRInputManager(osg::ref_ptr<OpenXRManager> XR);
        ~OpenXRInputManager();

        void updateControls();

        std::unique_ptr<OpenXRInputManagerImpl> mPrivate;
    };
}

#endif
