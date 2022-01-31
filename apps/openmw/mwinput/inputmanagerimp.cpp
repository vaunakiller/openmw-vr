#include "inputmanagerimp.hpp"

#include <osgViewer/ViewerEventHandlers>

#include <components/sdlutil/sdlinputwrapper.hpp>
#include <components/esm3/esmwriter.hpp>
#include <components/esm3/esmreader.hpp>

#include "../mwbase/windowmanager.hpp"
#include "../mwbase/environment.hpp"
#include "../mwbase/world.hpp"

#include "../mwworld/esmstore.hpp"

#include "actionmanager.hpp"
#include "bindingsmanager.hpp"
#include "controllermanager.hpp"
#include "controlswitch.hpp"
#include "keyboardmanager.hpp"
#include "mousemanager.hpp"
#include "sensormanager.hpp"
#include "gyromanager.hpp"

namespace MWInput
{
    InputManager::InputManager(
            SDL_Window* window,
            osg::ref_ptr<osgViewer::Viewer> viewer,
            osg::ref_ptr<osgViewer::ScreenCaptureHandler> screenCaptureHandler,
            osgViewer::ScreenCaptureHandler::CaptureOperation *screenCaptureOperation,
            const std::string& userFile, bool userFileExists, const std::string& userControllerBindingsFile,
            const std::string& controllerBindingsFile, bool grab)
        : mControlsDisabled(false)
    {
        mInputWrapper = new SDLUtil::InputWrapper(window, viewer, grab);
        mInputWrapper->setWindowEventCallback(MWBase::Environment::get().getWindowManager());

        mBindingsManager = new BindingsManager(userFile, userFileExists);

        mControlSwitch = new ControlSwitch();

        mActionManager = new ActionManager(mBindingsManager, screenCaptureOperation, viewer, screenCaptureHandler);

        mKeyboardManager = new KeyboardManager(mBindingsManager);
        mInputWrapper->setKeyboardEventCallback(mKeyboardManager);

        mMouseManager = new MouseManager(mBindingsManager, mInputWrapper, window);
        mInputWrapper->setMouseEventCallback(mMouseManager);

        mControllerManager = new ControllerManager(mBindingsManager, mActionManager, mMouseManager, userControllerBindingsFile, controllerBindingsFile);
        mInputWrapper->setControllerEventCallback(mControllerManager);

        mSensorManager = new SensorManager();
        mInputWrapper->setSensorEventCallback(mSensorManager);

        mGyroManager = new GyroManager();
    }

    void InputManager::clear()
    {
        // Enable all controls
        mControlSwitch->clear();
    }

    InputManager::~InputManager()
    {
        delete mActionManager;
        delete mControllerManager;
        delete mKeyboardManager;
        delete mMouseManager;
        delete mSensorManager;

        delete mControlSwitch;

        delete mBindingsManager;

        delete mInputWrapper;

        delete mGyroManager;
    }

    void InputManager::setAttemptJump(bool jumping)
    {
        mActionManager->setAttemptJump(jumping);
    }

    void InputManager::update(float dt, bool disableControls, bool disableEvents)
    {
        mControlsDisabled = disableControls;

        mInputWrapper->setMouseVisible(MWBase::Environment::get().getWindowManager()->getCursorVisible());
        mInputWrapper->capture(disableEvents);

        if (disableControls)
        {
            mMouseManager->updateCursorMode();
            return;
        }

        mBindingsManager->update(dt);

        mMouseManager->updateCursorMode();

        bool controllerMove = mControllerManager->update(dt);
        mMouseManager->update(dt);
        mSensorManager->update(dt);
        mActionManager->update(dt, controllerMove);

        if (mGyroManager->isEnabled())
        {
            bool controllerAvailable = mControllerManager->isGyroAvailable();
            bool sensorAvailable = mSensorManager->isGyroAvailable();
            if (controllerAvailable || sensorAvailable)
            {
                mGyroManager->update(dt,
                    controllerAvailable ? mControllerManager->getGyroValues() : mSensorManager->getGyroValues());
            }
        }
    }

    void InputManager::setDragDrop(bool dragDrop)
    {
        mBindingsManager->setDragDrop(dragDrop);
    }

    void InputManager::setGamepadGuiCursorEnabled(bool enabled)
    {
        mControllerManager->setGamepadGuiCursorEnabled(enabled);
    }

    void InputManager::changeInputMode(bool guiMode)
    {
        mControllerManager->setGuiCursorEnabled(guiMode);
        mMouseManager->setGuiCursorEnabled(guiMode);
        mGyroManager->setGuiCursorEnabled(guiMode);
        mMouseManager->setMouseLookEnabled(!guiMode);
        if (guiMode)
            MWBase::Environment::get().getWindowManager()->showCrosshair(false);

        bool isCursorVisible = guiMode && (!mControllerManager->joystickLastUsed() || mControllerManager->gamepadGuiCursorEnabled());
        MWBase::Environment::get().getWindowManager()->setCursorVisible(isCursorVisible);
        // if not in gui mode, the camera decides whether to show crosshair or not.
    }

    void InputManager::processChangedSettings(const Settings::CategorySettingVector& changed)
    {
        mMouseManager->processChangedSettings(changed);
        mSensorManager->processChangedSettings(changed);
        mGyroManager->processChangedSettings(changed);
    }

    bool InputManager::getControlSwitch(std::string_view sw)
    {
        return mControlSwitch->get(sw);
    }

    void InputManager::toggleControlSwitch(std::string_view sw, bool value)
    {
        mControlSwitch->set(sw, value);
    }

    void InputManager::resetIdleTime()
    {
        mActionManager->resetIdleTime();
    }

    bool InputManager::isIdle() const
    {
        return mActionManager->getIdleTime() > 0.5;
    }

    std::string InputManager::getActionDescription(int action) const
    {
        return mBindingsManager->getActionDescription(action);
    }

    std::string InputManager::getActionKeyBindingName(int action) const
    {
        return mBindingsManager->getActionKeyBindingName(action);
    }

    std::string InputManager::getActionControllerBindingName(int action) const
    {
        return mBindingsManager->getActionControllerBindingName(action);
    }

    bool InputManager::actionIsActive(int action) const
    {
        return mBindingsManager->actionIsActive(action);
    }

    float InputManager::getActionValue(int action) const
    {
        return mBindingsManager->getActionValue(action);
    }

    bool InputManager::isControllerButtonPressed(SDL_GameControllerButton button) const
    {
        return mControllerManager->isButtonPressed(button);
    }

    float InputManager::getControllerAxisValue(SDL_GameControllerAxis axis) const
    {
        return mControllerManager->getAxisValue(axis);
    }

    int InputManager::getMouseMoveX() const
    {
        return mMouseManager->getMouseMoveX();
    }

    int InputManager::getMouseMoveY() const
    {
        return mMouseManager->getMouseMoveY();
    }

    std::vector<int> InputManager::getActionKeySorting()
    {
        return mBindingsManager->getActionKeySorting();
    }

    std::vector<int> InputManager::getActionControllerSorting()
    {
        return mBindingsManager->getActionControllerSorting();
    }

    void InputManager::enableDetectingBindingMode(int action, bool keyboard)
    {
        mBindingsManager->enableDetectingBindingMode(action, keyboard);
    }

    int InputManager::countSavedGameRecords() const
    {
        return mControlSwitch->countSavedGameRecords();
    }

    void InputManager::write(ESM::ESMWriter& writer, Loading::Listener& progress)
    {
        mControlSwitch->write(writer, progress);
    }

    void InputManager::readRecord(ESM::ESMReader& reader, uint32_t type)
    {
        if (type == ESM::REC_INPU)
        {
            mControlSwitch->readRecord(reader, type);
        }
    }

    void InputManager::resetToDefaultKeyBindings()
    {
        mBindingsManager->loadKeyDefaults(true);
    }

    void InputManager::resetToDefaultControllerBindings()
    {
        mBindingsManager->loadControllerDefaults(true);
    }

    void InputManager::setJoystickLastUsed(bool enabled)
    {
        mControllerManager->setJoystickLastUsed(enabled);
    }

    bool InputManager::joystickLastUsed()
    {
        return mControllerManager->joystickLastUsed();
    }

    void InputManager::executeAction(int action)
    {
        mActionManager->executeAction(action);
    }
}
