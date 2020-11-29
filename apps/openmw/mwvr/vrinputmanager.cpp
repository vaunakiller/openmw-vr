#include "vrinputmanager.hpp"

#include "vrviewer.hpp"
#include "vrcamera.hpp"
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
        return activeActionSet().getLimbPose(time, limb);
    }

    OpenXRActionSet& VRInputManager::activeActionSet()
    {
        bool guiMode = MWBase::Environment::get().getWindowManager()->isGuiMode();
        guiMode = guiMode || (MWBase::Environment::get().getStateManager()->getState() == MWBase::StateManager::State_NoGame);
        if (guiMode)
        {
            return mXRInput->getActionSet(ActionSet::GUI);
        }
        return mXRInput->getActionSet(ActionSet::Gameplay);
    }

    void VRInputManager::updateActivationIndication(void)
    {
        bool guiMode = MWBase::Environment::get().getWindowManager()->isGuiMode();
        bool show = guiMode | mActivationIndication;
        auto* playerAnimation = Environment::get().getPlayerAnimation();
        if (playerAnimation)
        {
            playerAnimation->setFingerPointingMode(show);
        }
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
        if (Environment::get().getGUIManager()->injectMouseClick(onPress))
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
            activeActionSet().applyHaptics(TrackedLimb::LEFT_HAND, intensity);
    }

    void VRInputManager::applyHapticsRightHand(float intensity)
    {
        if (mHapticsEnabled)
            activeActionSet().applyHaptics(TrackedLimb::RIGHT_HAND, intensity);
    }

    void VRInputManager::processChangedSettings(const std::set<std::pair<std::string, std::string>>& changed)
    {
        MWInput::InputManager::processChangedSettings(changed);

        for (Settings::CategorySettingVector::const_iterator it = changed.begin(); it != changed.end(); ++it)
        {
            if (it->first == "VR" && it->second == "haptics enabled")
            {
                mHapticsEnabled = Settings::Manager::getBool("haptics enabled", "VR");
            }
        }
    }

    void VRInputManager::suggestBindingsSimple()
    {
        std::string simpleProfilePath = "/interaction_profiles/khr/simple_controller";
        // Set up default bindings for the khronos simple controller.
        // Note: The simple controller is the equivalent to a universal "default".
        // It has highly reduced functionality. Only poses and two click actions
        // are available for each hand, reducing the possible functionality of the profile
        // to that of a wonky preview.
        // The available click actions are 'select' and 'menu', and i cannot control what
        // real buttons this is mapped to. On the Oculus Touch they are X, Y, A, and B.

        // In-game character controls
        SuggestedBindings simpleGameplayBindings{
                {MWInput::A_Use,        "/user/hand/left/input/select/click"}, // Touch: X
                {A_VrMetaMenu,          "/user/hand/left/input/menu/click"}, // Touch: Y
                {A_Recenter,            "/user/hand/left/input/menu/click"}, // Touch: Y
                {A_ActivateTouch,       "/user/hand/right/input/select/click"}, // Touch: A
                {MWInput::A_AutoMove,   "/user/hand/right/input/menu/click"}, // Touch: B
        };

        // GUI controls
        SuggestedBindings simpleGUIBindings{
                {MWInput::A_Use,        "/user/hand/left/input/select/click"}, // Touch: X
                {MWInput::A_GameMenu,   "/user/hand/left/input/menu/click"}, // Touch: Y
                {A_Recenter,            "/user/hand/left/input/menu/click"}, // Touch: Y
                {A_MenuSelect,          "/user/hand/right/input/select/click"}, // Touch: A
                {A_MenuBack,            "/user/hand/right/input/menu/click"}, // Touch: B
        };
        mXRInput->suggestBindings(ActionSet::Gameplay, simpleProfilePath, simpleGameplayBindings);
        mXRInput->suggestBindings(ActionSet::GUI, simpleProfilePath, simpleGUIBindings);
    }

    void VRInputManager::suggestBindingsOculusTouch()
    {
        std::string controllerProfilePath = "/interaction_profiles/oculus/touch_controller";

        // In-game character controls
        SuggestedBindings gameplayBindings{
                {A_Recenter,                    "/user/hand/left/input/menu/click"},
                {A_VrMetaMenu,                  "/user/hand/left/input/menu/click"},
                {MWInput::A_Sneak,              "/user/hand/left/input/squeeze/value"},
                {MWInput::A_MoveLeftRight,      "/user/hand/left/input/thumbstick/x"},
                {MWInput::A_MoveForwardBackward,"/user/hand/left/input/thumbstick/y"},
                {MWInput::A_AlwaysRun,          "/user/hand/left/input/thumbstick/click"},
                {MWInput::A_Jump,               "/user/hand/left/input/trigger/value"},
                {MWInput::A_ToggleSpell,        "/user/hand/left/input/x/click"},
                {MWInput::A_Rest,               "/user/hand/left/input/y/click"},
                {MWInput::A_ToggleWeapon,       "/user/hand/right/input/a/click"},
                {MWInput::A_Inventory,          "/user/hand/right/input/b/click"},
                {A_ActivateTouch,               "/user/hand/right/input/squeeze/value"},
                {MWInput::A_Activate,           "/user/hand/right/input/squeeze/value"},
                {MWInput::A_LookLeftRight,      "/user/hand/right/input/thumbstick/x"},
                {MWInput::A_AutoMove,           "/user/hand/right/input/thumbstick/click"},
                {MWInput::A_Use,                "/user/hand/right/input/trigger/value"},
        };

        // GUI controls
        SuggestedBindings GUIBindings{
                {A_MenuUpDown,          "/user/hand/right/input/thumbstick/y"},
                {A_MenuLeftRight,       "/user/hand/right/input/thumbstick/x"},
                {A_MenuSelect,          "/user/hand/right/input/a/click"},
                {A_MenuBack,            "/user/hand/right/input/b/click"},
                {MWInput::A_Use,        "/user/hand/right/input/trigger/value"},
                {MWInput::A_GameMenu,   "/user/hand/left/input/menu/click"},
                {A_Recenter,            "/user/hand/left/input/menu/click"},
        };

        mXRInput->suggestBindings(ActionSet::Gameplay, controllerProfilePath, gameplayBindings);
        mXRInput->suggestBindings(ActionSet::GUI, controllerProfilePath, GUIBindings);
    }

    void VRInputManager::suggestBindingsHpMixedReality()
    {
        std::string controllerProfilePath = "/interaction_profiles/hp/mixed_reality_controller";

        // In-game character controls
        SuggestedBindings gameplayBindings{
                {A_Recenter,                    "/user/hand/left/input/menu/click"},
                {A_VrMetaMenu,                  "/user/hand/left/input/menu/click"},
                {MWInput::A_Sneak,              "/user/hand/left/input/squeeze/value"},
                {MWInput::A_MoveForwardBackward,"/user/hand/left/input/thumbstick/y"},
                {MWInput::A_MoveLeftRight,      "/user/hand/left/input/thumbstick/x"},
                {MWInput::A_AlwaysRun,          "/user/hand/left/input/thumbstick/click"},
                {MWInput::A_Jump,               "/user/hand/left/input/trigger/value"},
                {MWInput::A_ToggleSpell,        "/user/hand/left/input/x/click"},
                {MWInput::A_Rest,               "/user/hand/left/input/y/click"},
                {MWInput::A_ToggleWeapon,       "/user/hand/right/input/a/click"},
                {MWInput::A_Inventory,          "/user/hand/right/input/b/click"},
                {MWInput::A_LookLeftRight,      "/user/hand/right/input/thumbstick/x"},
                {MWInput::A_AutoMove,           "/user/hand/right/input/thumbstick/click"},
                {MWInput::A_Use,                "/user/hand/right/input/trigger/value"},
                {A_ActivateTouch,               "/user/hand/right/input/squeeze/value"},
                {MWInput::A_Activate,           "/user/hand/right/input/squeeze/value"},
        };

        // GUI controls
        SuggestedBindings GUIBindings{
                {A_Recenter,            "/user/hand/left/input/menu/click"},
                {MWInput::A_GameMenu,   "/user/hand/left/input/menu/click"},
                {A_MenuUpDown,          "/user/hand/right/input/thumbstick/y"},
                {A_MenuLeftRight,       "/user/hand/right/input/thumbstick/x"},
                {A_MenuSelect,          "/user/hand/right/input/a/click"},
                {A_MenuBack,            "/user/hand/right/input/b/click"},
                {MWInput::A_Use,        "/user/hand/right/input/trigger/value"},
        };

        mXRInput->suggestBindings(ActionSet::Gameplay, controllerProfilePath, gameplayBindings);
        mXRInput->suggestBindings(ActionSet::GUI, controllerProfilePath, GUIBindings);
    }

    void VRInputManager::suggestBindingsHuaweiController()
    {
        std::string controllerProfilePath = "/interaction_profiles/huawei/controller";

        // In-game character controls
        SuggestedBindings gameplayBindings{
            {A_Recenter,                    "/user/hand/left/input/home/click"},
            {A_VrMetaMenu,                  "/user/hand/left/input/home/click"},
            {MWInput::A_Jump,               "/user/hand/left/input/trigger/click"},
            {MWInput::A_MoveForwardBackward,"/user/hand/left/input/trackpad/y"},
            {MWInput::A_MoveLeftRight,      "/user/hand/left/input/trackpad/x"},
            {MWInput::A_ToggleSpell,        "/user/hand/left/input/trackpad/click"},
            {MWInput::A_Sneak,              "/user/hand/left/input/back/click"},
            {MWInput::A_LookLeftRight,      "/user/hand/right/input/trackpad/x"},
            {MWInput::A_ToggleWeapon,       "/user/hand/right/input/trackpad/click"},
            {MWInput::A_Use,                "/user/hand/right/input/trigger/click"},
            {A_ActivateTouch,               "/user/hand/right/input/squeeze/click"},
            {MWInput::A_Activate,           "/user/hand/right/input/squeeze/click"},
        };

        // GUI controls
        SuggestedBindings GUIBindings{
                {A_MenuBack,            "/user/hand/left/input/trackpad/click"},
                {MWInput::A_GameMenu,   "/user/hand/left/input/home/click"},
                {A_Recenter,            "/user/hand/left/input/home/click"},
                {A_MenuUpDown,          "/user/hand/right/input/thumbstick/y"},
                {A_MenuLeftRight,       "/user/hand/right/input/thumbstick/x"},
                {A_MenuSelect,          "/user/hand/right/input/trackpad/click"},
                {MWInput::A_Use,        "/user/hand/right/input/trigger/click"},
        };

        mXRInput->suggestBindings(ActionSet::Gameplay, controllerProfilePath, gameplayBindings);
        mXRInput->suggestBindings(ActionSet::GUI, controllerProfilePath, GUIBindings);
    }

    void VRInputManager::suggestBindingsMicrosoftMixedReality()
    {
        std::string controllerProfilePath = "/interaction_profiles/microsoft/motion_controller";

        // In-game character controls
        SuggestedBindings gameplayBindings{
            {A_Recenter,                    "/user/hand/left/input/menu/click"},
            {A_VrMetaMenu,                  "/user/hand/left/input/menu/click"},
            {MWInput::A_Jump,               "/user/hand/left/input/trigger/value"},
            {MWInput::A_MoveForwardBackward,"/user/hand/left/input/trackpad/y"},
            {MWInput::A_MoveLeftRight,      "/user/hand/left/input/trackpad/x"},
            {MWInput::A_Rest,               "/user/hand/left/input/thumbstick/click"},
            {MWInput::A_ToggleSpell,        "/user/hand/left/input/trackpad/click"},
            {MWInput::A_Sneak,              "/user/hand/left/input/squeeze/click"},
            {MWInput::A_Inventory,          "/user/hand/right/input/thumbstick/click"},
            {MWInput::A_LookLeftRight,      "/user/hand/right/input/trackpad/x"},
            {MWInput::A_ToggleWeapon,       "/user/hand/right/input/trackpad/click"},
            {MWInput::A_Use,                "/user/hand/right/input/trigger/value"},
            {A_ActivateTouch,               "/user/hand/right/input/squeeze/click"},
            {MWInput::A_Activate,           "/user/hand/right/input/squeeze/click"},
        };

        // GUI controls
        SuggestedBindings GUIBindings{
                {A_MenuBack,            "/user/hand/left/input/trackpad/click"},
                {MWInput::A_GameMenu,   "/user/hand/left/input/menu/click"},
                {A_Recenter,            "/user/hand/left/input/menu/click"},
                {A_MenuUpDown,          "/user/hand/right/input/trackpad/y"},
                {A_MenuLeftRight,       "/user/hand/right/input/trackpad/x"},
                {A_MenuSelect,          "/user/hand/right/input/trackpad/click"},
                {MWInput::A_Use,        "/user/hand/right/input/trigger/value"},
        };

        mXRInput->suggestBindings(ActionSet::Gameplay, controllerProfilePath, gameplayBindings);
        mXRInput->suggestBindings(ActionSet::GUI, controllerProfilePath, GUIBindings);
    }

    void VRInputManager::suggestBindingsIndex()
    {
        std::string controllerProfilePath = "/interaction_profiles/valve/index_controller";
        // In-game character controls
        SuggestedBindings gameplayBindings{
                {MWInput::A_ToggleSpell,        "/user/hand/left/input/a/click"},
                {MWInput::A_Rest,               "/user/hand/left/input/b/click"},
                {MWInput::A_MoveForwardBackward,"/user/hand/left/input/thumbstick/y"},
                {MWInput::A_MoveLeftRight,      "/user/hand/left/input/thumbstick/x"},
                {A_Recenter,                    "/user/hand/left/input/trackpad/force"},
                {A_VrMetaMenu,                  "/user/hand/left/input/trackpad/force"},
                {MWInput::A_Jump,               "/user/hand/left/input/trigger/value"},
                {MWInput::A_Sneak,              "/user/hand/left/input/squeeze/force"},
                {MWInput::A_ToggleWeapon,       "/user/hand/right/input/a/click"},
                {MWInput::A_Inventory,          "/user/hand/right/input/b/click"},
                {MWInput::A_LookLeftRight,      "/user/hand/right/input/thumbstick/x"},
                {MWInput::A_Use,                "/user/hand/right/input/trigger/value"},
                {A_ActivateTouch,               "/user/hand/right/input/squeeze/force"},
                {MWInput::A_Activate,           "/user/hand/right/input/squeeze/force"},
        };

        // GUI controls
        SuggestedBindings GUIBindings{
                {A_Recenter,            "/user/hand/left/input/thumbstick/click"},
                {MWInput::A_GameMenu,   "/user/hand/left/input/trackpad/force"},
                {A_MenuSelect,          "/user/hand/right/input/a/click"},
                {A_MenuBack,            "/user/hand/right/input/b/click"},
                {MWInput::A_Use,        "/user/hand/right/input/trigger/value"},
                {A_MenuUpDown,          "/user/hand/right/input/thumbstick/y"},
                {A_MenuLeftRight,       "/user/hand/right/input/thumbstick/x"},
        };

        mXRInput->suggestBindings(ActionSet::Gameplay, controllerProfilePath, gameplayBindings);
        mXRInput->suggestBindings(ActionSet::GUI, controllerProfilePath, GUIBindings);
    }

    void VRInputManager::suggestBindingsVive()
    {
        std::string controllerProfilePath = "/interaction_profiles/htc/vive_controller";

        // In-game character controls
        SuggestedBindings gameplayBindings{
            {A_Recenter,                    "/user/hand/left/input/menu/click"},
            {A_VrMetaMenu,                  "/user/hand/left/input/menu/click"},
            {A_VrMetaMenu,                  "/user/hand/right/input/squeeze/click"},
            {MWInput::A_MoveForwardBackward,"/user/hand/left/input/trackpad/y"},
            {MWInput::A_MoveLeftRight,      "/user/hand/left/input/trackpad/x"},
            {MWInput::A_ToggleSpell,        "/user/hand/left/input/trackpad/click"},
            {MWInput::A_Jump,               "/user/hand/left/input/trigger/value"},
            {MWInput::A_Sneak,              "/user/hand/left/input/squeeze/click"},
            {MWInput::A_LookLeftRight,      "/user/hand/right/input/trackpad/x"},
            {MWInput::A_ToggleWeapon,       "/user/hand/right/input/trackpad/click"},
            {MWInput::A_Use,                "/user/hand/right/input/trigger/value"},
            {A_ActivateTouch,               "/user/hand/right/input/squeeze/click"},
            {MWInput::A_Activate,           "/user/hand/right/input/squeeze/click"},
        };

        // GUI controls
        SuggestedBindings GUIBindings{
                {A_MenuUpDown,          "/user/hand/right/input/trackpad/y"},
                {A_MenuLeftRight,       "/user/hand/right/input/trackpad/x"},
                {A_MenuSelect,          "/user/hand/right/input/trackpad/click"},
                {A_MenuBack,            "/user/hand/left/input/trackpad/click"},
                {MWInput::A_GameMenu,   "/user/hand/left/input/menu/click"},
                {MWInput::A_Use,        "/user/hand/right/input/trigger/value"},
                {A_Recenter,            "/user/hand/left/input/menu/click"},
        };

        mXRInput->suggestBindings(ActionSet::Gameplay, controllerProfilePath, gameplayBindings);
        mXRInput->suggestBindings(ActionSet::GUI, controllerProfilePath, GUIBindings);
    }

    void VRInputManager::suggestBindingsViveCosmos()
    {
        std::string controllerProfilePath = "/interaction_profiles/htc/vive_cosmos_controller";

        // In-game character controls
        SuggestedBindings gameplayBindings{
                {A_Recenter,                    "/user/hand/left/input/menu/click"},
                {A_VrMetaMenu,                  "/user/hand/left/input/menu/click"},
                {MWInput::A_Sneak,              "/user/hand/left/input/squeeze/value"},
                {MWInput::A_MoveForwardBackward,"/user/hand/left/input/thumbstick/y"},
                {MWInput::A_MoveLeftRight,      "/user/hand/left/input/thumbstick/x"},
                {MWInput::A_AlwaysRun,          "/user/hand/left/input/thumbstick/click"},
                {MWInput::A_Jump,               "/user/hand/left/input/trigger/click"},
                {MWInput::A_ToggleSpell,        "/user/hand/left/input/x/click"},
                {MWInput::A_Rest,               "/user/hand/left/input/y/click"},
                {MWInput::A_ToggleWeapon,       "/user/hand/right/input/a/click"},
                {MWInput::A_Inventory,          "/user/hand/right/input/b/click"},
                {MWInput::A_LookLeftRight,      "/user/hand/right/input/thumbstick/x"},
                {MWInput::A_AutoMove,           "/user/hand/right/input/thumbstick/click"},
                {MWInput::A_Use,                "/user/hand/right/input/trigger/click"},
                {A_ActivateTouch,               "/user/hand/right/input/squeeze/value"},
                {MWInput::A_Activate,           "/user/hand/right/input/squeeze/value"},
        };

        // GUI controls
        SuggestedBindings GUIBindings{
                {A_Recenter,            "/user/hand/left/input/menu/click"},
                {MWInput::A_GameMenu,   "/user/hand/left/input/menu/click"},
                {A_MenuUpDown,          "/user/hand/right/input/thumbstick/y"},
                {A_MenuLeftRight,       "/user/hand/right/input/thumbstick/x"},
                {A_MenuSelect,          "/user/hand/right/input/a/click"},
                {A_MenuBack,            "/user/hand/right/input/b/click"},
                {MWInput::A_Use,        "/user/hand/right/input/trigger/click"},
        };

        mXRInput->suggestBindings(ActionSet::Gameplay, controllerProfilePath, gameplayBindings);
        mXRInput->suggestBindings(ActionSet::GUI, controllerProfilePath, GUIBindings);
    }

    void VRInputManager::suggestBindingsXboxController()
    {
        //TODO
    }

    void VRInputManager::requestRecenter()
    {
        // TODO: Hack, should have a cleaner way of accessing this
        reinterpret_cast<VRCamera*>(MWBase::Environment::get().getWorld()->getRenderingManager().getCamera())->requestRecenter();
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
        , mXRInput(new OpenXRInput)
        , mHapticsEnabled{ Settings::Manager::getBool("haptics enabled", "VR") }
    {
        auto xr = MWVR::Environment::get().getManager();

        suggestBindingsSimple();
        suggestBindingsOculusTouch();
        suggestBindingsMicrosoftMixedReality();
        suggestBindingsIndex();
        suggestBindingsVive();
        suggestBindingsXboxController();

        if (xr->xrExtensionIsEnabled(XR_EXT_HP_MIXED_REALITY_CONTROLLER_EXTENSION_NAME))
            suggestBindingsHpMixedReality();
        if (xr->xrExtensionIsEnabled(XR_HUAWEI_CONTROLLER_INTERACTION_EXTENSION_NAME))
            suggestBindingsHuaweiController();
        if (xr->xrExtensionIsEnabled(XR_HTC_VIVE_COSMOS_CONTROLLER_INTERACTION_EXTENSION_NAME))
            suggestBindingsViveCosmos();

        mXRInput->attachActionSets();
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
        auto& actionSet = activeActionSet();
        actionSet.updateControls();

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

        while (auto* action = actionSet.nextAction())
        {
            processAction(action, dt, disableControls);
        }

        updateActivationIndication();

        MWInput::InputManager::update(dt, disableControls, disableEvents);
        // This is the first update that needs openxr tracking, so i begin the next frame here.
        auto* session = Environment::get().getSession();
        if (!session)
            return;
        
        session->beginPhase(VRSession::FramePhase::Update);

        // The rest of this code assumes the game is running
        if (MWBase::Environment::get().getStateManager()->getState() == MWBase::StateManager::State_NoGame)
            return;

        bool guiMode = MWBase::Environment::get().getWindowManager()->isGuiMode();

        // OpenMW assumes all input will come via SDL which i often violate.
        // This keeps player controls correctly enabled for my purposes.
        mBindingsManager->setPlayerControlsEnabled(!guiMode);

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
        auto* wm = MWBase::Environment::get().getWindowManager();


        // OpenMW does not currently provide any way to directly request skipping a video.
        // This is copied from the controller manager and is used to skip videos, 
        // and works because mygui only consumes the escape press if a video is currently playing.
        if (wm->isPlayingVideo())
        {
            auto kc = MWInput::sdlKeyToMyGUI(SDLK_ESCAPE);
            if (action->onActivate())
            {
                mBindingsManager->setPlayerControlsEnabled(!MyGUI::InputManager::getInstance().injectKeyPress(kc, 0));
            }
            else if (action->onDeactivate())
            {
                mBindingsManager->setPlayerControlsEnabled(!MyGUI::InputManager::getInstance().injectKeyRelease(kc));
            }
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
                    break;
                case A_MenuSelect:
                    if (!wm->injectKeyPress(MyGUI::KeyCode::Return, 0, false))
                        executeAction(MWInput::A_Activate);
                    break;
                case A_MenuBack:
                    if (MyGUI::InputManager::getInstance().isModalAny())
                        wm->exitCurrentModal();
                    else
                        wm->exitCurrentGuiMode();
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
            {
                float yaw = osg::DegreesToRadians(action->value()) * 200.f * dt;
                // TODO: Hack, should have a cleaner way of accessing this
                reinterpret_cast<VRCamera*>(MWBase::Environment::get().getWorld()->getRenderingManager().getCamera())->rotateStage(yaw);
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
                default:
                    break;
                }
            }
        }
    }
}
