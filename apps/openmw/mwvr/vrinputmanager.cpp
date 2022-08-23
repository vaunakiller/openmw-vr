#include "vrinputmanager.hpp"

#include "vranimation.hpp"
#include "vrcamera.hpp"
#include "vrgui.hpp"
#include "vrpointer.hpp"
#include "openxrinput.hpp"
#include "realisticcombat.hpp"

#include <components/sdlutil/sdlmappings.hpp>
#include <components/debug/debuglog.hpp>
#include <components/xr/instance.hpp>
#include <components/xr/action.hpp>
#include <components/xr/actionset.hpp>
#include <components/vr/trackingmanager.hpp>

#include <MyGUI_InputManager.h>

#include "../mwbase/world.hpp"
#include "../mwbase/windowmanager.hpp"
#include "../mwbase/statemanager.hpp"
#include "../mwbase/environment.hpp"
#include "../mwbase/mechanicsmanager.hpp"

#include "../mwgui/draganddrop.hpp"
#include "../mwgui/inventorywindow.hpp"

#include "../mwinput/actionmanager.hpp"
#include "../mwinput/bindingsmanager.hpp"
#include "../mwinput/mousemanager.hpp"

#include "../mwworld/player.hpp"
#include "../mwmechanics/actorutil.hpp"

#include "../mwrender/renderingmanager.hpp"
#include "../mwrender/camera.hpp"

#include <extern/oics/ICSInputControlSystem.h>

#include <iostream>

namespace MWVR
{
    XR::ActionSet& VRInputManager::activeActionSet()
    {
        bool guiMode = MWBase::Environment::get().getWindowManager()->isGuiMode();
        guiMode = guiMode || (MWBase::Environment::get().getStateManager()->getState() == MWBase::StateManager::State_NoGame);
        if (guiMode)
        {
            return mXRInput->getActionSet(MWActionSet::GUI);
        }
        return mXRInput->getActionSet(MWActionSet::Gameplay);
    }

    void VRInputManager::notifyInteractionProfileChanged()
    {
        mXRInput->notifyInteractionProfileChanged();
    }

    void VRInputManager::updateVRPointer(void)
    {
        bool guiMode = MWBase::Environment::get().getWindowManager()->isGuiMode();
        MWBase::Environment::get().getWorld()->enableVRPointer(guiMode || mPointerLeft, guiMode || mPointerRight);
    }

    /**
     * Makes it possible to use ItemModel::moveItem to move an item from an inventory to the world.
     */
    class DropItemAtPointModel : public MWGui::ItemModel
    {
    public:
        DropItemAtPointModel() {}
        ~DropItemAtPointModel() {}
        MWWorld::Ptr copyItem(const MWGui::ItemStack& item, size_t count, bool /*allowAutoEquip*/) override
        {
            MWBase::World* world = MWBase::Environment::get().getWorld();
            auto pointer = MWVR::VRGUIManager::instance().getUserPointer();

            MWWorld::Ptr dropped;
            if (pointer->canPlaceObject())
                dropped = world->placeObject(item.mBase, pointer->getPointerRay(), count);
            else
                dropped = world->dropObjectOnGround(world->getPlayerPtr(), item.mBase, count);
            dropped.getCellRef().setOwner("");

            return dropped;
        }

        void removeItem(const MWGui::ItemStack& item, size_t count) override { throw std::runtime_error("removeItem not implemented"); }
        ModelIndex getIndex(const MWGui::ItemStack& item) override { throw std::runtime_error("getIndex not implemented"); }
        void update() override {}
        size_t getItemCount() override { return 0; }
        MWGui::ItemStack getItem(ModelIndex index) override{ throw std::runtime_error("getItem not implemented"); }
        bool usesContainer(const MWWorld::Ptr&) override { return false; }

    private:
        // Where to drop the item
        MWRender::RayResult mIntersection;
    };

    void VRInputManager::pointActivation(bool onPress)
    {
        if (controlsDisabled())
        {
            injectMousePress(SDL_BUTTON_LEFT, onPress);
            return;
        }

        auto pointer = MWVR::VRGUIManager::instance().getUserPointer();
        if (pointer->getPointerRay().mHit)
        {
            auto* node = pointer->getPointerRay().mHitNode;
            MWWorld::Ptr ptr = pointer->getPointerRay().mHitObject;
            auto wm = MWBase::Environment::get().getWindowManager();
            auto& dnd = wm->getDragAndDrop();

            if (node && node->getName() == "VRGUILayer")
            {
                injectMousePress(SDL_BUTTON_LEFT, onPress);
            }
            else if (onPress)
            {
                // Other actions should only happen on release;
                return;
            }
            else if (dnd.mIsOnDragAndDrop)
            {
                // Intersected with the world while drag and drop is active
                // Drop item into the world
                MWBase::Environment::get().getWorld()->breakInvisibility(
                    MWMechanics::getPlayer());
                DropItemAtPointModel drop;
                dnd.drop(&drop, nullptr);
            }
            else if (!ptr.isEmpty())
            {
                if (wm->isConsoleMode())
                    wm->setConsoleSelectedObject(ptr);
                // Don't active things during GUI mode.
                else if (wm->isGuiMode())
                {
                    if (wm->getMode() != MWGui::GM_Container && wm->getMode() != MWGui::GM_Inventory)
                        return;
                    wm->getInventoryWindow()->pickUpObject(ptr);
                }
                else
                {
                    MWWorld::Player& player = MWBase::Environment::get().getWorld()->getPlayer();
                    player.activate(ptr);
                }
            }
        }
    }

    void VRInputManager::injectMousePress(int sdlButton, bool onPress)
    {
        if (MWVR::VRGUIManager::instance().injectMouseClick(onPress))
            return;

        SDL_MouseButtonEvent arg;
        if (onPress)
            mMouseManager->mousePressed(arg, sdlButton);
        else
            mMouseManager->mouseReleased(arg, sdlButton);
    }

    void VRInputManager::injectChannelValue(
        MWInput::Actions action,
        float value)
    {
        auto channel = mBindingsManager->ics().getChannel(MWInput::A_MoveLeftRight);// ->setValue(value);
        channel->setEnabled(true);
    }

    void VRInputManager::applyHapticsLeftHand(float intensity)
    {
        if (mHapticsEnabled)
            mXRInput->getActionSet(MWActionSet::Haptics).applyHaptics(VR::SubAction::HandLeft, intensity);
    }

    void VRInputManager::applyHapticsRightHand(float intensity)
    {
        if (mHapticsEnabled)
            mXRInput->getActionSet(MWActionSet::Haptics).applyHaptics(VR::SubAction::HandRight, intensity);
    }

    void VRInputManager::processChangedSettings(const std::set<std::pair<std::string, std::string>>& changed)
    {
        MWInput::InputManager::processChangedSettings(changed);

        for (Settings::CategorySettingVector::const_iterator it = changed.begin(); it != changed.end(); ++it)
        {
            if (it->first == "VR" && it->second == "snap angle")
            {
                mSnapAngle = Settings::Manager::getFloat("snap angle", "VR");
                Log(Debug::Verbose) << "Snap angle set to: " << mSnapAngle;
            }
            if (it->first == "VR" && it->second == "smooth turn rate")
            {
                mSmoothTurnRate = Settings::Manager::getFloat("smooth turn rate", "VR");
                Log(Debug::Verbose) << "Smooth turning rate set to: " << mSmoothTurnRate;
            }
            if (it->first == "VR" && it->second == "smooth turning")
            {
                mSmoothTurning = Settings::Manager::getBool("smooth turning", "VR");
                Log(Debug::Verbose) << "Smooth turning set to: " << mSmoothTurning;
            }
            if (it->first == "VR" && it->second == "haptics enabled")
            {
                mHapticsEnabled = Settings::Manager::getBool("haptics enabled", "VR");
            }
            if (it->first == "Input" && it->second == "joystick dead zone")
            {
                setThumbstickDeadzone(Settings::Manager::getFloat("joystick dead zone", "Input"));
            }
        }
    }

    void VRInputManager::setThumbstickDeadzone(float deadzoneRadius)
    {
        mXRInput->setThumbstickDeadzone(deadzoneRadius);
    }

    void VRInputManager::turnLeftRight(const XR::InputAction* action, float dt)
    {
        auto path = VR::stringToVRPath("/world/user");
        auto* stageToWorldBinding = static_cast<VR::StageToWorldBinding*>(VR::TrackingManager::instance().getTrackingSource(path));
        if (mSmoothTurning)
        {
            float yaw = osg::DegreesToRadians(action->value()) * smoothTurnRate(dt);
            // TODO: Hack, should have a cleaner way of accessing this
            stageToWorldBinding->setWorldOrientation(yaw, true);
        }
        else
        {
            if (action->value() > 0.6f && action->previousValue() < 0.6f)
            {
                stageToWorldBinding->setWorldOrientation(osg::DegreesToRadians(mSnapAngle), true);
            }
            if (action->value() < -0.6f && action->previousValue() > -0.6f)
            {
                stageToWorldBinding->setWorldOrientation(-osg::DegreesToRadians(mSnapAngle), true);
            }
        }
    }

    float VRInputManager::smoothTurnRate(float dt) const
    {
        return 360.f * mSmoothTurnRate * dt;
    }

    void VRInputManager::calibrate()
    {
        if (!Settings::Manager::getBool("player height calibrated", "VR"))
            calibratePlayerHeight();

    }
    void VRInputManager::calibratePlayerHeight()
    {
        auto wm = MWBase::Environment::get().getWindowManager();
        mCalibrationState = CalibrationState::Active;

        wm->staticMessageBox("To deal with the diversity in height of playable races, OpenMW-VR Needs to know how tall you are. Stand up straight and press the menu key. You'll be able to redo this calibrating later by going to the VR tab of the settings menu");

        struct HeightListener : public VR::TrackingListener
        {
            float height = 1.8;
            bool receivedTrackingData = false;
            VR::VRPath path = VR::stringToVRPath("/stage/user/head/input/pose");
            
            void onTrackingUpdated(VR::TrackingManager& manager, VR::DisplayTime predictedDisplayTime) override
            {
                auto pose = manager.locate(path, predictedDisplayTime);
                if (static_cast<int>(pose.status) > 0)
                {
                    receivedTrackingData = true;
                    height = pose.pose.position.z();
                    Log(Debug::Verbose) << "Height: " << height;
                }
            }
        } heightListener;

        while (mCalibrationState == CalibrationState::Active)
        {
            if (MWBase::Environment::get().getStateManager()->hasQuitRequest())
            {
                mCalibrationState = CalibrationState::Aborted;
            }
            else
            {
                // Note, we ignore framerate limiting here, since OpenXR takes care of that.
                MWBase::Environment::get().getInputManager()->update(0.f, true, false);

                wm->viewerTraversals();
            }
        }

        wm->removeStaticMessageBox();

        // Game is exiting, do not care about completing calibrationg.
        if (mCalibrationState == CalibrationState::Aborted)
            return;

        float height = 1.8;

        if (mCalibrationState != CalibrationState::Complete)
        {
            wm->messageBox("Calibration was skipped. Using a default height of 1.8m. You should redo the calibration later by going to the VR tab of the settings menu");
        }
        else if (!heightListener.receivedTrackingData)
        {
            wm->messageBox("Could not read tracking data. Using a default height of 1.8m. You should redo the calibration later by going to the VR tab of the settings menu");
        }
        else
        {
            height = heightListener.height;
        }

        Settings::Manager::setFloat("player height", "VR", height);
        Settings::Manager::setBool("player height calibrated", "VR", true);
    }

    void VRInputManager::requestRecenter(bool resetZ)
    {
        // TODO: Hack, should have a cleaner way of accessing this
        reinterpret_cast<VRCamera*>(MWBase::Environment::get().getWorld()->getRenderingManager()->getCamera())->requestRecenter(resetZ);
    }

    VRInputManager::VRInputManager(
        SDL_Window* window,
        osg::ref_ptr<osgViewer::Viewer> viewer,
        osg::ref_ptr<osgViewer::ScreenCaptureHandler> screenCaptureHandler,
        osgViewer::ScreenCaptureHandler::CaptureOperation* screenCaptureOperation,
        const std::string& userFile,
        bool userFileExists,
        const std::string& userControllerBindingsFile,
        const std::string& controllerBindingsFile,
        bool grab,
        const std::string& xrControllerSuggestionsFile)
        : MWInput::InputManager(
            window,
            viewer,
            screenCaptureHandler,
            screenCaptureOperation,
            userFile,
            userFileExists,
            userControllerBindingsFile,
            controllerBindingsFile,
            grab)
        , mXRInput(new OpenXRInput(xrControllerSuggestionsFile))
        , mHapticsEnabled{ Settings::Manager::getBool("haptics enabled", "VR") }
        , mSmoothTurning{ Settings::Manager::getBool("smooth turning", "VR") }
        , mSnapAngle{ Settings::Manager::getFloat("snap angle", "VR") }
        , mSmoothTurnRate{ Settings::Manager::getFloat("smooth turn rate", "VR") }
    {
        setThumbstickDeadzone(Settings::Manager::getFloat("joystick dead zone", "Input"));
    }

    VRInputManager::~VRInputManager()
    {
    }

    void VRInputManager::changeInputMode(bool mode)
    {
        // VR mode has no concept of these
        //mGuiCursorEnabled = false;
        MWInput::InputManager::changeInputMode(mode);
        MWBase::Environment::get().getWindowManager()->showCrosshair(false);
        MWBase::Environment::get().getWindowManager()->setCursorVisible(false);
    }

    void VRInputManager::update(
        float dt,
        bool disableControls,
        bool disableEvents)
    {
        auto& actionSet = activeActionSet();
        actionSet.updateControls();

        bool vrHasFocus = MWVR::VRGUIManager::instance().updateFocus();
        auto guiCursor = MWVR::VRGUIManager::instance().guiCursor();
        if (vrHasFocus)
        {
            mMouseManager->setMousePosition(guiCursor.x(), guiCursor.y());
        }

        while (auto* action = actionSet.nextAction())
        {
            processAction(action, dt, disableControls);
        }


        MWInput::InputManager::update(dt, disableControls, disableEvents);

        // The rest of this code assumes the game is running
        if (MWBase::Environment::get().getStateManager()->getState() == MWBase::StateManager::State_NoGame)
        {
            MWVR::VRGUIManager::instance().updateTracking();
            return;
        }

        updateVRPointer();
        bool guiMode = MWBase::Environment::get().getWindowManager()->isGuiMode();

        // OpenMW assumes all input will come via SDL which i often violate.
        // This keeps player controls correctly enabled for my purposes.
        mBindingsManager->setPlayerControlsEnabled(!guiMode);

        if (!guiMode)
        {
            auto world = MWBase::Environment::get().getWorld();

            auto& player = world->getPlayer();
            auto playerPtr = world->getPlayerPtr();
            if (!mRealisticCombat || mRealisticCombat->ptr() != playerPtr)
            {
                // TODO: de-hardcode right-handedness for when ability to equip weapons with left hand is completed
                auto trackingPath = VR::stringToVRPath("/stage/user/hand/right/input/aim/pose");
                mRealisticCombat.reset(new RealisticCombat::StateMachine(playerPtr, trackingPath));
            }
            bool enabled = !guiMode && player.getDrawState() == MWMechanics::DrawState::Weapon && !player.isDisabled();
            mRealisticCombat->update(dt, enabled);
        }
        else if (mRealisticCombat)
            mRealisticCombat->update(dt, false);


        // Update tracking every frame if player is not currently in GUI mode.
        // This ensures certain widgets like Notifications will be visible.
        if (!guiMode)
        {
            MWVR::VRGUIManager::instance().updateTracking();
        }
    }

    void VRInputManager::processAction(const XR::InputAction* action, float dt, bool disableControls)
    {
        static const bool isToggleSneak = Settings::Manager::getBool("toggle sneak", "Input");
        auto wm = MWBase::Environment::get().getWindowManager();

        // OpenMW does not currently provide any way to directly request skipping a video.
        // This is copied from the controller manager and is used to skip videos, 
        // and works because mygui only consumes the escape press if a video is currently playing.
        if (wm->isPlayingVideo())
        {
            auto kc = SDLUtil::sdlKeyToMyGUI(SDLK_ESCAPE);
            if (action->onActivate())
            {
                mBindingsManager->setPlayerControlsEnabled(!MyGUI::InputManager::getInstance().injectKeyPress(kc, 0));
            }
            else if (action->onDeactivate())
            {
                mBindingsManager->setPlayerControlsEnabled(!MyGUI::InputManager::getInstance().injectKeyRelease(kc));
            }
        }

        if (mCalibrationState == CalibrationState::Active)
        {
            if (action->onDeactivate() && action->openMWActionCode() == MWInput::A_GameMenu)
            {
                mCalibrationState = CalibrationState::Complete;
            }
        }

        bool guiMode = MWBase::Environment::get().getWindowManager()->isGuiMode();

        if (guiMode)
        {
            MyGUI::KeyCode key = MyGUI::KeyCode::None;
            bool onPress = true;

            // Axis actions
            switch (action->openMWActionCode())
            {
            case A_MenuLeftRight:
                if (action->value() > 0.6f && action->previousValue() < 0.6f)
                {
                    key = MyGUI::KeyCode::ArrowRight;
                }
                if (action->value() < -0.6f && action->previousValue() > -0.6f)
                {
                    key = MyGUI::KeyCode::ArrowLeft;
                }
                if (action->value() < 0.6f && action->previousValue() > 0.6f)
                {
                    key = MyGUI::KeyCode::ArrowRight;
                    onPress = false;
                }
                if (action->value() > -0.6f && action->previousValue() < -0.6f)
                {
                    key = MyGUI::KeyCode::ArrowLeft;
                    onPress = false;
                }
                break;
            case A_MenuUpDown:
                if (action->value() > 0.6f && action->previousValue() < 0.6f)
                {
                    key = MyGUI::KeyCode::ArrowUp;
                }
                if (action->value() < -0.6f && action->previousValue() > -0.6f)
                {
                    key = MyGUI::KeyCode::ArrowDown;
                }
                if (action->value() < 0.6f && action->previousValue() > 0.6f)
                {
                    key = MyGUI::KeyCode::ArrowUp;
                    onPress = false;
                }
                if (action->value() > -0.6f && action->previousValue() < -0.6f)
                {
                    key = MyGUI::KeyCode::ArrowDown;
                    onPress = false;
                }
                break;
            default: break;
            }

            // OnActivate actions
            if (action->onActivate())
            {
                switch (action->openMWActionCode())
                {
                case MWInput::A_GameMenu:
                    mActionManager->toggleMainMenu();
                    break;
                case MWInput::A_Screenshot:
                    mActionManager->screenshot();
                    break;
                case A_Recenter:
                    MWVR::VRGUIManager::instance().updateTracking();
                    break;
                case A_MenuSelect:
                    wm->injectKeyPress(MyGUI::KeyCode::Return, 0, false);
                    break;
                case A_MenuBack:
                    if (MyGUI::InputManager::getInstance().isModalAny())
                        wm->exitCurrentModal();
                    else
                        wm->exitCurrentGuiMode();
                    break;
                case MWInput::A_Use:
                    pointActivation(true);
                    break;
                default:
                    break;
                }
            }

            // A few actions need to fire on deactivation
            if (action->onDeactivate())
            {
                switch (action->openMWActionCode())
                {
                case MWInput::A_Use:
                    mBindingsManager->ics().getChannel(MWInput::A_Use)->setValue(0.f);
                    pointActivation(false);
                    break;
                case A_MenuSelect:
                    wm->injectKeyRelease(MyGUI::KeyCode::Return);
                    break;
                default:
                    break;
                }
            }

            if (key != MyGUI::KeyCode::None)
            {
                if (onPress)
                {
                    MWBase::Environment::get().getWindowManager()->injectKeyPress(key, 0, 0);
                }
                else
                {
                    MWBase::Environment::get().getWindowManager()->injectKeyRelease(key);
                }
            }
        }

        else
        {
            if (disableControls)
            {
                return;
            }

            // Hold actions
            switch (action->openMWActionCode())
            {
            case A_ActivateTouch:
                resetIdleTime();

                if (action->subAction() == VR::SubAction::HandLeft)
                    mPointerLeft = action->isActive();
                if (action->subAction() == VR::SubAction::HandRight)
                    mPointerRight = action->isActive();
                break;
            case MWInput::A_LookLeftRight:
            {
                turnLeftRight(action, dt);
                break;
            }
            case MWInput::A_MoveLeftRight:
                mBindingsManager->ics().getChannel(MWInput::A_MoveLeftRight)->setValue(action->value() / 2.f + 0.5f);
                break;
            case MWInput::A_MoveForwardBackward:
                mBindingsManager->ics().getChannel(MWInput::A_MoveForwardBackward)->setValue(-action->value() / 2.f + 0.5f);
                break;
            case MWInput::A_Sneak:
            {
                if (!isToggleSneak)
                    mBindingsManager->ics().getChannel(MWInput::A_Sneak)->setValue(action->isActive() ? 1.f : 0.f);
                break;
            }
            case MWInput::A_Use:
                if (!(mPointerLeft || mPointerRight || MWBase::Environment::get().getWindowManager()->isGuiMode()))
                    mBindingsManager->ics().getChannel(MWInput::A_Use)->setValue(action->value());
                break;
            default:
                break;
            }

            // OnActivate actions
            if (action->onActivate())
            {
                switch (action->openMWActionCode())
                {
                case MWInput::A_GameMenu:
                    mActionManager->toggleMainMenu();
                    break;
                case A_VrMetaMenu:
                    MWBase::Environment::get().getWindowManager()->pushGuiMode(MWGui::GM_VrMetaMenu);
                    break;
                case MWInput::A_Screenshot:
                    mActionManager->screenshot();
                    break;
                case MWInput::A_Inventory:
                    mActionManager->toggleInventory();
                    break;
                case MWInput::A_Console:
                    mActionManager->toggleConsole();
                    break;
                case MWInput::A_Journal:
                    mActionManager->toggleJournal();
                    break;
                case MWInput::A_AutoMove:
                    mActionManager->toggleAutoMove();
                    break;
                case MWInput::A_AlwaysRun:
                    mActionManager->toggleWalking();
                    break;
                case MWInput::A_ToggleWeapon:
                    mActionManager->toggleWeapon();
                    break;
                case MWInput::A_Rest:
                    mActionManager->rest();
                    break;
                case MWInput::A_ToggleSpell:
                    mActionManager->toggleSpell();
                    break;
                case MWInput::A_QuickKey1:
                    mActionManager->quickKey(1);
                    break;
                case MWInput::A_QuickKey2:
                    mActionManager->quickKey(2);
                    break;
                case MWInput::A_QuickKey3:
                    mActionManager->quickKey(3);
                    break;
                case MWInput::A_QuickKey4:
                    mActionManager->quickKey(4);
                    break;
                case MWInput::A_QuickKey5:
                    mActionManager->quickKey(5);
                    break;
                case MWInput::A_QuickKey6:
                    mActionManager->quickKey(6);
                    break;
                case MWInput::A_QuickKey7:
                    mActionManager->quickKey(7);
                    break;
                case MWInput::A_QuickKey8:
                    mActionManager->quickKey(8);
                    break;
                case MWInput::A_QuickKey9:
                    mActionManager->quickKey(9);
                    break;
                case MWInput::A_QuickKey10:
                    mActionManager->quickKey(10);
                    break;
                case MWInput::A_QuickKeysMenu:
                    mActionManager->showQuickKeysMenu();
                    break;
                case MWInput::A_ToggleHUD:
                    Log(Debug::Verbose) << "Toggle HUD";
                    MWBase::Environment::get().getWindowManager()->toggleHud();
                    break;
                case MWInput::A_ToggleDebug:
                    Log(Debug::Verbose) << "Toggle Debug";
                    MWBase::Environment::get().getWindowManager()->toggleDebugWindow();
                    break;
                case MWInput::A_QuickSave:
                    mActionManager->quickSave();
                    break;
                case MWInput::A_QuickLoad:
                    mActionManager->quickLoad();
                    break;
                case MWInput::A_CycleSpellLeft:
                    if (mActionManager->checkAllowedToUseItems() && MWBase::Environment::get().getWindowManager()->isAllowed(MWGui::GW_Magic))
                        MWBase::Environment::get().getWindowManager()->cycleSpell(false);
                    break;
                case MWInput::A_CycleSpellRight:
                    if (mActionManager->checkAllowedToUseItems() && MWBase::Environment::get().getWindowManager()->isAllowed(MWGui::GW_Magic))
                        MWBase::Environment::get().getWindowManager()->cycleSpell(true);
                    break;
                case MWInput::A_CycleWeaponLeft:
                    if (mActionManager->checkAllowedToUseItems() && MWBase::Environment::get().getWindowManager()->isAllowed(MWGui::GW_Inventory))
                        MWBase::Environment::get().getWindowManager()->cycleWeapon(false);
                    break;
                case MWInput::A_CycleWeaponRight:
                    if (mActionManager->checkAllowedToUseItems() && MWBase::Environment::get().getWindowManager()->isAllowed(MWGui::GW_Inventory))
                        MWBase::Environment::get().getWindowManager()->cycleWeapon(true);
                    break;
                case MWInput::A_Jump:
                    mActionManager->setAttemptJump(true);
                    break;
                case A_Recenter:
                    MWVR::VRGUIManager::instance().updateTracking();
                    if (!MWBase::Environment::get().getWindowManager()->isGuiMode())
                        requestRecenter(true);
                    break;
                case MWInput::A_Use:
                    if (mPointerLeft || mPointerRight || MWBase::Environment::get().getWindowManager()->isGuiMode())
                        pointActivation(true);
                    break;
                default:
                    break;
                }
            }

            // A few actions need to fire on deactivation
            if (action->onDeactivate())
            {
                switch (action->openMWActionCode())
                {
                case MWInput::A_Use:
                    mBindingsManager->ics().getChannel(MWInput::A_Use)->setValue(0.f);
                    if (mPointerLeft || mPointerRight || MWBase::Environment::get().getWindowManager()->isGuiMode())
                        pointActivation(false);
                    break;
                case MWInput::A_Sneak:
                    if (isToggleSneak)
                        mActionManager->toggleSneaking();
                    break;
                default:
                    break;
                }
            }
        }
    }
}
