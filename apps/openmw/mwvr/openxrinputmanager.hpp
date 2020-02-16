#ifndef OPENXR_INPUT_MANAGER_HPP
#define OPENXR_INPUT_MANAGER_HPP

#include "openxrviewer.hpp"
#include "../mwinput/inputmanagerimp.hpp"

#include <vector>
#include <array>
#include <iostream>

namespace MWVR
{
    struct OpenXRInput;
    struct OpenXRActionEvent;

    /// As far as I can tell, SDL does not support VR controllers.
    /// So I subclass the input manager and insert VR controls.
    class OpenXRInputManager : public MWInput::InputManager
    {
    public:
        OpenXRInputManager(
            SDL_Window* window,
            osg::ref_ptr<OpenXRViewer> viewer,
            osg::ref_ptr<osgViewer::ScreenCaptureHandler> screenCaptureHandler,
            osgViewer::ScreenCaptureHandler::CaptureOperation* screenCaptureOperation,
            const std::string& userFile, bool userFileExists,
            const std::string& userControllerBindingsFile,
            const std::string& controllerBindingsFile, bool grab);

        virtual ~OpenXRInputManager();

        /// Overriden to always disallow mouselook and similar.
        virtual void changeInputMode(bool guiMode);

        /// Overriden to update XR inputs
        virtual void update(float dt, bool disableControls = false, bool disableEvents = false);

        void processEvent(const OpenXRActionEvent& event);

        PoseSet getHandPoses(int64_t time, TrackedSpace space);

        void showActivationIndication(bool show);

        osg::ref_ptr<OpenXRViewer>   mXRViewer;
        std::unique_ptr<OpenXRInput> mXRInput;
    };
}

#endif
