#include "vrinputmanager.hpp"

#include "vrviewer.hpp"
#include "vrgui.hpp"
#include "vranimation.hpp"
#include "openxrinput.hpp"
#include "vrenvironment.hpp"
#include "openxrmanager.hpp"
#include "openxrmanagerimpl.hpp"
#include "openxraction.hpp"
#include "realisticcombat.hpp"

#include <components/debug/debuglog.hpp>

#include <MyGUI_InputManager.h>

#include "../mwbase/world.hpp"
#include "../mwbase/windowmanager.hpp"
#include "../mwbase/statemanager.hpp"
#include "../mwbase/environment.hpp"
#include "../mwbase/mechanicsmanager.hpp"

#include "../mwgui/draganddrop.hpp"

#include "../mwinput/actionmanager.hpp"
#include "../mwinput/bindingsmanager.hpp"
#include "../mwinput/mousemanager.hpp"
#include "../mwinput/sdlmappings.hpp"

#include "../mwworld/player.hpp"
#include "../mwmechanics/actorutil.hpp"

#include "../mwrender/renderingmanager.hpp"
#include "../mwrender/camera.hpp"

#include <extern/oics/ICSInputControlSystem.h>

#include <iostream>

namespace MWVR
{

    Pose VRInputManager::getLimbPose(int64_t time, TrackedLimb limb)
    {
        return mXRInput->getLimbPose(time, limb);
    }

    void VRInputManager::updateActivationIndication(void)
    {
        bool guiMode = MWBase::Environment::get().getWindowManager()->isGuiMode();
        bool show = guiMode | mActivationIndication;
        auto* playerAnimation = Environment::get().getPlayerAnimation();
        if (playerAnimation)
            playerAnimation->setFingerPointingMode(show);
    }


    /**
     * Makes it possible to use ItemModel::moveItem to move an item from an inventory to the world.
     */
    class DropItemAtPointModel : public MWGui::ItemModel
    {
    public:
        DropItemAtPointModel() {}
        virtual ~DropItemAtPointModel() {}
        virtual MWWorld::Ptr copyItem(const MWGui::ItemStack& item, size_t count, bool /*allowAutoEquip*/)
        {
            MWBase::World* world = MWBase::Environment::get().getWorld();
            MWVR::VRAnimation* anim = MWVR::Environment::get().getPlayerAnimation();

            MWWorld::Ptr dropped;
            if (anim->canPlaceObject())
                dropped = world->placeObject(item.mBase, anim->getPointerTarget(), count);
            else
                dropped = world->dropObjectOnGround(world->getPlayerPtr(), item.mBase, count);
            dropped.getCellRef().setOwner("");

            return dropped;
        }

        virtual void removeItem(const MWGui::ItemStack& item, size_t count) { throw std::runtime_error("removeItem not implemented"); }
        virtual ModelIndex getIndex(MWGui::ItemStack item) { throw std::runtime_error("getIndex not implemented"); }
        virtual void update() {}
        virtual size_t getItemCount() { return 0; }
        virtual MWGui::ItemStack getItem(ModelIndex index) { throw std::runtime_error("getItem not implemented"); }

    private:
        // Where to drop the item
        MWRender::RayResult mIntersection;
    };

    void VRInputManager::pointActivation(bool onPress)
    {
        auto* world = MWBase::Environment::get().getWorld();
        auto* anim = MWVR::Environment::get().getPlayerAnimation();
        if (world && anim && anim->getPointerTarget().mHit)
        {
            auto* node = anim->getPointerTarget().mHitNode;
            MWWorld::Ptr ptr = anim->getPointerTarget().mHitObject;
            auto& dnd = MWBase::Environment::get().getWindowManager()->getDragAndDrop();

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
                MWWorld::Player& player = MWBase::Environment::get().getWorld()->getPlayer();
                player.activate(ptr);
            }
        }
    }

    void VRInputManager::injectMousePress(int sdlButton, bool onPress)
    {
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
            mXRInput->applyHaptics(TrackedLimb::LEFT_HAND, intensity);
    }

    void VRInputManager::applyHapticsRightHand(float intensity)
    {
        if (mHapticsEnabled)
            mXRInput->applyHaptics(TrackedLimb::RIGHT_HAND, intensity);
    }

    void VRInputManager::requestRecenter()
    {
        mShouldRecenter = true;
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
        bool grab)
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
        , mXRInput(nullptr)
        , mHapticsEnabled{Settings::Manager::getBool("haptics enabled", "VR")}
    {
        std::vector<SuggestedBindings> suggestedBindings;
        // Set up default bindings for the oculus

        /*
            Oculus Bindings:
            L-Squeeze:
                Hold: Sneak

            R-Squeeze:
                Hold: Enable Pointer

            L-Trigger:
                Press: Jump

            R-Trigger:
                IF POINTER:
                    Activate
                ELSE:
                    Use

            L-Thumbstick:
                X-Axis: MoveForwardBackward
                Y-Axis: MoveLeftRight
                Button:
                    Press: AlwaysRun
                    Long: ToggleHUD
                Touch:

            R-Thumbstick:
                X-Axis: LookLeftRight
                Y-Axis:
                Button:
                    Press: AutoMove
                    Long: ToggleDebug
                Touch:

            X:
                Press: Toggle Spell
                Long:
            Y:
                Press: Rest
                Long: Quick Save
            A:
                Press: Toggle Weapon
                Long:
            B:
                Press: Inventory
                Long: Journal

            Menu:
                Press: GameMenu
                Long: Recenter on player and reset GUI

        */
        SuggestedBindings oculusTouchBindings{
            "/interaction_profiles/oculus/touch_controller",
            {
                {A_MenuUpDown, ActionPath::ThumbstickY, Side::RIGHT_SIDE},
                {A_MenuLeftRight, ActionPath::ThumbstickX, Side::RIGHT_SIDE},
                {A_MenuSelect, ActionPath::A, Side::RIGHT_SIDE},
                {A_MenuBack, ActionPath::B, Side::RIGHT_SIDE},
                {MWInput::A_LookLeftRight, ActionPath::ThumbstickX, Side::RIGHT_SIDE},
                {MWInput::A_MoveLeftRight, ActionPath::ThumbstickX, Side::LEFT_SIDE},
                {MWInput::A_MoveForwardBackward, ActionPath::ThumbstickY, Side::LEFT_SIDE},
                {MWInput::A_Activate, ActionPath::Squeeze, Side::RIGHT_SIDE},
                {MWInput::A_Use, ActionPath::Trigger, Side::RIGHT_SIDE},
                {MWInput::A_Jump, ActionPath::Trigger, Side::LEFT_SIDE},
                {MWInput::A_ToggleWeapon, ActionPath::A, Side::RIGHT_SIDE},
                {MWInput::A_ToggleSpell, ActionPath::X, Side::LEFT_SIDE},
                //{*mCycleSpellLeft, mThumbstickClickPath, Side::LEFT_SIDE},
                //{*mCycleSpellRight, mThumbstickClickPath, Side::RIGHT_SIDE},
                //{*mCycleWeaponLeft, mThumbstickClickPath, Side::LEFT_SIDE},
                //{*mCycleWeaponRight, mThumbstickClickPath, Side::RIGHT_SIDE},
                {MWInput::A_AlwaysRun, ActionPath::ThumbstickClick, Side::LEFT_SIDE},
                {MWInput::A_AutoMove, ActionPath::ThumbstickClick, Side::RIGHT_SIDE},
                {MWInput::A_ToggleHUD, ActionPath::ThumbstickClick, Side::LEFT_SIDE},
                {MWInput::A_ToggleDebug, ActionPath::ThumbstickClick, Side::RIGHT_SIDE},
                {MWInput::A_Sneak, ActionPath::Squeeze, Side::LEFT_SIDE},
                {MWInput::A_Inventory, ActionPath::B, Side::RIGHT_SIDE},
                {MWInput::A_Rest, ActionPath::Y, Side::LEFT_SIDE},
                {MWInput::A_Journal, ActionPath::B, Side::RIGHT_SIDE},
                {MWInput::A_QuickSave, ActionPath::Y, Side::LEFT_SIDE},
                {MWInput::A_GameMenu, ActionPath::Menu, Side::LEFT_SIDE},
                {A_Recenter, ActionPath::Menu, Side::LEFT_SIDE},
                {A_ActivateTouch, ActionPath::Squeeze, Side::RIGHT_SIDE},
            }
        };

        suggestedBindings.push_back(oculusTouchBindings);

        mXRInput.reset(new OpenXRInput(suggestedBindings));
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
        auto begin = std::chrono::steady_clock::now();
        mXRInput->updateControls();


        auto* vrGuiManager = Environment::get().getGUIManager();
        if (vrGuiManager)
        {
            bool vrHasFocus = vrGuiManager->updateFocus();
            auto guiCursor = vrGuiManager->guiCursor();
            if (vrHasFocus)
            {
                mMouseManager->setMousePosition(guiCursor.x(), guiCursor.y());
            }
        }

        while (auto* action = mXRInput->nextAction())
        {
            processAction(action, dt, disableControls);
        }

        updateActivationIndication();

        MWInput::InputManager::update(dt, disableControls, disableEvents);

        // This is the first update that needs openxr tracking, so i begin the next frame here.
        auto* session = Environment::get().getSession();
        if (session)
            session->beginPhase(VRSession::FramePhase::Update);

        // The rest of this code assumes the game is running
        if (MWBase::Environment::get().getStateManager()->getState() == MWBase::StateManager::State_NoGame)
        {
            return;
        }

        bool guiMode = MWBase::Environment::get().getWindowManager()->isGuiMode();

        // OpenMW assumes all input will come via SDL which i often violate.
        // This keeps player controls correctly enabled for my purposes.
        mBindingsManager->setPlayerControlsEnabled(!guiMode);

        updateHead();

        if (!guiMode)
        {
            auto* world = MWBase::Environment::get().getWorld();

            auto& player = world->getPlayer();
            auto playerPtr = world->getPlayerPtr();
            if (!mRealisticCombat || mRealisticCombat->ptr() != playerPtr)
                mRealisticCombat.reset(new RealisticCombat::StateMachine(playerPtr));
            bool enabled = !guiMode && player.getDrawState() == MWMechanics::DrawState_Weapon && !player.isDisabled();
            mRealisticCombat->update(dt, enabled);
        }
        else if (mRealisticCombat)
            mRealisticCombat->update(dt, false);


        // Update tracking every frame if player is not currently in GUI mode.
        // This ensures certain widgets like Notifications will be visible.
        if (!guiMode)
        {
            vrGuiManager->updateTracking();
        }
        auto end = std::chrono::steady_clock::now();

        auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(end - begin);
    }

    void VRInputManager::processAction(const Action* action, float dt, bool disableControls)
    {
        static const bool isToggleSneak = Settings::Manager::getBool("toggle sneak", "Input");
        auto* vrGuiManager = Environment::get().getGUIManager();


        // OpenMW does not currently provide any way to directly request skipping a video.
        // This is copied from the controller manager and is used to skip videos, 
        // and works because mygui only consumes the escape press if a video is currently playing.
        auto kc = MWInput::sdlKeyToMyGUI(SDLK_ESCAPE);
        if (action->onActivate())
        {
            mBindingsManager->setPlayerControlsEnabled(!MyGUI::InputManager::getInstance().injectKeyPress(kc, 0));
        }
        else if (action->onDeactivate())
        {
            mBindingsManager->setPlayerControlsEnabled(!MyGUI::InputManager::getInstance().injectKeyRelease(kc));
        }

        if (disableControls)
        {
            return;
        }

        bool guiMode = MWBase::Environment::get().getWindowManager()->isGuiMode();

        if (guiMode)
        {
            MyGUI::KeyCode key = MyGUI::KeyCode::None;

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
                    vrGuiManager->updateTracking();
                    requestRecenter();
                    break;
                case A_MenuSelect:
                    if (!MWBase::Environment::get().getWindowManager()->injectKeyPress(MyGUI::KeyCode::Space, 0, 0))
                        executeAction(MWInput::A_Activate);
                    break;
                case A_MenuBack:
                    if (MyGUI::InputManager::getInstance().isModalAny())
                        MWBase::Environment::get().getWindowManager()->exitCurrentModal();
                    else
                        MWBase::Environment::get().getWindowManager()->exitCurrentGuiMode();
                    break;
                case MWInput::A_Use:
                    pointActivation(true);
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
                default:
                    break;
                }
            }

            if (key != MyGUI::KeyCode::None)
            {
                MWBase::Environment::get().getWindowManager()->injectKeyPress(key, 0, 0);
            }
        }

        else
        {

            // Hold actions
            switch (action->openMWActionCode())
            {
            case A_ActivateTouch:
                resetIdleTime();
                mActivationIndication = action->isActive();
                break;
            case MWInput::A_LookLeftRight:
                mYaw += osg::DegreesToRadians(action->value()) * 200.f * dt;
                break;
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
                if (!(mActivationIndication || MWBase::Environment::get().getWindowManager()->isGuiMode()))
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
                case MWInput::A_Screenshot:
                    mActionManager->screenshot();
                    break;
                case MWInput::A_Inventory:
                    //mActionManager->toggleInventory();
                    injectMousePress(SDL_BUTTON_RIGHT, true);
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
                    vrGuiManager->updateTracking();
                    if (!MWBase::Environment::get().getWindowManager()->isGuiMode())
                        requestRecenter();
                    break;
                case MWInput::A_Use:
                    if (mActivationIndication || MWBase::Environment::get().getWindowManager()->isGuiMode())
                        pointActivation(true);
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
                    if (mActivationIndication || MWBase::Environment::get().getWindowManager()->isGuiMode())
                        pointActivation(false);
                    break;
                case MWInput::A_Sneak:
                    if (isToggleSneak)
                        mActionManager->toggleSneaking();
                    break;
                case MWInput::A_Inventory:
                    injectMousePress(SDL_BUTTON_RIGHT, false);
                default:
                    break;
                }
            }
        }
    }

    osg::Quat VRInputManager::stageRotation()
    {
        return osg::Quat(mYaw, osg::Vec3(0, 0, -1));
    }

    void VRInputManager::updateHead()
    {
        auto* session = Environment::get().getSession();
        auto currentHeadPose = session->predictedPoses(VRSession::FramePhase::Update).head;
        currentHeadPose.position *= Environment::get().unitsPerMeter();
        osg::Vec3 vrMovement = currentHeadPose.position - mHeadPose.position;
        mHeadPose = currentHeadPose;
        mHeadOffset += stageRotation() * vrMovement;

        if (mShouldRecenter)
        {
            // Move position of head to center of character 
            // Z should not be affected
            mHeadOffset = osg::Vec3(0, 0, 0);
            mHeadOffset.z() = mHeadPose.position.z();

            // Adjust orientation to zero yaw
            float yaw = 0.f;
            float pitch = 0.f;
            float roll = 0.f;
            getEulerAngles(mHeadPose.orientation, yaw, pitch, roll);
            mYaw = -yaw;

            mShouldRecenter = false;
            Log(Debug::Verbose) << "Recentered (" << mYaw << ")";
        }
        else
        {
            MWBase::World* world = MWBase::Environment::get().getWorld();
            auto& player = world->getPlayer();
            auto playerPtr = player.getPlayer();

            float yaw = 0.f;
            float pitch = 0.f;
            float roll = 0.f;
            getEulerAngles(mHeadPose.orientation, yaw, pitch, roll);

            yaw += mYaw;

            mVrAngles[0] = pitch;
            mVrAngles[1] = roll;
            mVrAngles[2] = yaw;

            if (!player.isDisabled())
            {
                world->rotateObject(playerPtr, mVrAngles[0], mVrAngles[1], mVrAngles[2], MWBase::RotationFlag_none);
            }
            else {
                // Update the camera directly to avoid rotating the disabled player
                world->getRenderingManager().getCamera()->rotateCamera(-pitch, -roll, -yaw, false);
            }
        }

    }
}
