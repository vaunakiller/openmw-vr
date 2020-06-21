/////////////////////////////////////////////////
//
//  State machine for "realistic" combat in openmw vr
//
//  Initial state: Ready.
//
//  State Ready: Ready to initiate a new attack.
//  State Launch: Player has begun swinging his weapon.
//  State Swing: Currently swinging weapon.
//  State Impact: Contact made, weapon still swinging.
//  State Cooldown: Swing completed, wait a minimum period before next.
//
//  Transition rules:
//    Ready     -> Launch:      When the minimum velocity of swing is achieved.
//    Launch    -> Ready:       When the minimum velocity of swing is lost before minimum distance was swung.
//    Launch    -> Swing:       When minimum distance is swung.
//                                  - Play Swish sound
//    Swing     -> Impact:      When minimum velocity is lost, or when a hit is detected.
//                                  - Register hit based on max velocity observed in swing state
//    Impact    -> Cooldown:    When velocity returns below minimum.
//    Cooldown  -> Ready:       When the minimum period has passed since entering Cooldown state
//


#include "realisticcombat.hpp"

#include "../mwbase/environment.hpp"
#include "../mwbase/soundmanager.hpp"

#include "../mwmechanics/weapontype.hpp"

#include <components/debug/debuglog.hpp>

#include <iomanip>

namespace MWVR { namespace RealisticCombat {

static const char* stateToString(SwingState florida)
{
    switch (florida)
    {
    case SwingState_Cooldown:
        return "Cooldown";
    case SwingState_Impact:
        return "Impact";
    case SwingState_Ready:
        return "Ready";
    case SwingState_Swing:
        return "Swing";
    case SwingState_Launch:
        return "Launch";
    }
    return "Error, invalid enum";
}
static const char* swingTypeToString(int type)
{
    switch (type)
    {
    case ESM::Weapon::AT_Chop:
        return "Chop";
    case ESM::Weapon::AT_Slash:
        return "Slash";
    case ESM::Weapon::AT_Thrust:
        return "Thrust";
    case -1:
        return "Fail";
    default:
        return "Invalid";
    }
}

StateMachine::StateMachine(MWWorld::Ptr ptr) : ptr(ptr) {}

bool StateMachine::canSwing()
{
    if (swingType >= 0)
        if (velocity >= minVelocity)
            if (swingType != ESM::Weapon::AT_Thrust || thrustVelocity >= 0.f)
                return true;
    return false;
}

// Actions common to all transitions
void StateMachine::transition(
    SwingState newState)
{
    Log(Debug::Verbose) << "Transition:" << stateToString(state) << "->" << stateToString(newState);
    maxSwingVelocity = 0.f;
    timeSinceEnteredState = 0.f;
    movementSinceEnteredState = 0.f;
    state = newState;
}

void StateMachine::reset()
{
    maxSwingVelocity = 0.f;
    timeSinceEnteredState = 0.f;
    velocity = 0.f;
    previousPosition = osg::Vec3(0.f, 0.f, 0.f);
    state = SwingState_Ready;
}

static bool isMeleeWeapon(int type)
{
    if (MWMechanics::getWeaponType(type)->mWeaponClass != ESM::WeaponType::Melee)
        return false;
    if (type == ESM::Weapon::HandToHand)
        return true;
    if (type >= 0)
        return true;

    return false;
}

static bool isSideSwingValidForWeapon(int type)
{
    switch (type)
    {
    case ESM::Weapon::HandToHand:
    case ESM::Weapon::BluntOneHand:
    case ESM::Weapon::BluntTwoClose:
    case ESM::Weapon::BluntTwoWide:
    case ESM::Weapon::SpearTwoWide:
        return true;
    case ESM::Weapon::ShortBladeOneHand:
    case ESM::Weapon::LongBladeOneHand:
    case ESM::Weapon::LongBladeTwoHand:
    case ESM::Weapon::AxeOneHand:
    case ESM::Weapon::AxeTwoHand:
    default:
        return false;
    }
}

void StateMachine::update(float dt, bool enabled)
{
    auto* session = Environment::get().getSession();
    auto* world = MWBase::Environment::get().getWorld();
    auto& predictedPoses = session->predictedPoses(VRSession::FramePhase::Update);
    auto& handPose = predictedPoses.hands[(int)MWVR::Side::RIGHT_SIDE];
    auto weaponType = world->getActiveWeaponType();

    enabled = enabled && isMeleeWeapon(weaponType);

    if (mEnabled != enabled)
    {
        reset();
        mEnabled = enabled;
    }
    if (!enabled)
        return;

    timeSinceEnteredState += dt;


    // First determine direction of different swing types

    // Discover orientation of weapon
    osg::Quat weaponDir = handPose.orientation;

    // Morrowind models do not hold weapons at a natural angle, so i rotate the hand forward 
    // to get a more natural angle on the weapon to allow more comfortable combat.
    if (weaponType != ESM::Weapon::HandToHand)
        weaponDir = osg::Quat(osg::PI_4, osg::Vec3{ 1,0,0 }) * weaponDir;

    // Thrust means stabbing in the direction of the weapon
    osg::Vec3 thrustDirection = weaponDir * osg::Vec3{ 0,1,0 };

    // Slash and Chop are vertical, relative to the orientation of the weapon (direction of the sharp edge / hammer)
    osg::Vec3 slashChopDirection = weaponDir * osg::Vec3{ 0,0,1 };

    // Side direction of the weapon (i.e. The blunt side of the sword)
    osg::Vec3 sideDirection = weaponDir * osg::Vec3{ 1,0,0 };


    // Next determine current hand movement

    // If tracking is lost, openxr will return a position of 0
    // So i reset position when tracking is re-acquired to avoid a superspeed strike.
    // Theoretically, the player's hand really could be at 0,0,0
    // but that's a super rare case so whatever.
    if (previousPosition == osg::Vec3(0.f, 0.f, 0.f))
        previousPosition = handPose.position;

    osg::Vec3 movement = handPose.position - previousPosition;
    movementSinceEnteredState += movement.length();
    previousPosition = handPose.position;
    osg::Vec3 swingVector = movement / dt;
    osg::Vec3 swingDirection = swingVector;
    swingDirection.normalize();

    // Compute swing velocities

    // Thrust follows the orientation of the weapon. Negative thrust = no attack.
    thrustVelocity = swingVector * thrustDirection;
    velocity = swingVector.length();
    

    if (isSideSwingValidForWeapon(weaponType))
    {
        // Compute velocity in the plane normal to the thrust direction.
        float thrustComponent = std::abs(thrustVelocity / velocity);
        float planeComponent = std::sqrt(1 - thrustComponent * thrustComponent);
        slashChopVelocity = velocity * planeComponent;
        sideVelocity = -1000.f;
    }
    else
    {
        // If side swing is not valid for the weapon, count slash/chop only along in
        // the direction of the weapon's edge.
        slashChopVelocity = std::abs(swingVector * slashChopDirection);
        sideVelocity = std::abs(swingVector * sideDirection);
    }


    float orientationVerticality = std::abs(thrustDirection * osg::Vec3{ 0,0,1 });
    float swingVerticality = std::abs(swingDirection * osg::Vec3{ 0,0,1 });

    // Pick swing type based on greatest current velocity
    // Note i use abs() of thrust velocity to prevent accidentally triggering
    // chop/slash when player is withdrawing the weapon.
    if (sideVelocity > std::abs(thrustVelocity) && sideVelocity > slashChopVelocity)
    {
        // Player is swinging with the "blunt" side of a weapon that
        // cannot be used that way.
        swingType = -1;
    }
    else if (std::abs(thrustVelocity) > slashChopVelocity)
    {
        swingType = ESM::Weapon::AT_Thrust;
    }
    else
    {
        // First check if the weapon is pointing upwards. In which case slash is not 
        // applicable, and the attack must be a chop.
        if (orientationVerticality > 0.707)
            swingType = ESM::Weapon::AT_Chop;
        else
        {
            // Next check if the swing is more horizontal or vertical. A slash
            // would be more horizontal.
            if(swingVerticality > 0.707)
                swingType = ESM::Weapon::AT_Chop;
            else
                swingType = ESM::Weapon::AT_Slash;
        }
    }

    switch (state)
    {
    case SwingState_Cooldown:
        return update_cooldownState();
    case SwingState_Ready:
        return update_readyState();
    case SwingState_Swing:
        return update_swingState();
    case SwingState_Impact:
        return update_impactState();
    case SwingState_Launch:
        return update_launchState();
    default:
        throw std::logic_error(std::string("You forgot to implement state ") + stateToString(state) + " ya dingus");
    }
}

void StateMachine::update_cooldownState()
{
    if (timeSinceEnteredState >= minimumPeriod)
        transition_cooldownToReady();
}

void StateMachine::transition_cooldownToReady()
{
    transition(SwingState_Ready);
}

void StateMachine::update_readyState()
{
    if (canSwing())
        return transition_readyToLaunch();
}

void StateMachine::transition_readyToLaunch()
{
    transition(SwingState_Launch);
}

void StateMachine::playSwish()
{
    MWBase::SoundManager* sndMgr = MWBase::Environment::get().getSoundManager();

    std::string sound = "Weapon Swish";
    if (strength < 0.5f)
        sndMgr->playSound3D(ptr, sound, 1.0f, 0.8f); //Weak attack
    if (strength < 1.0f)
        sndMgr->playSound3D(ptr, sound, 1.0f, 1.0f); //Medium attack
    else
        sndMgr->playSound3D(ptr, sound, 1.0f, 1.2f); //Strong attack

    Log(Debug::Verbose) << "Swing: " << swingTypeToString(swingType);
}

void StateMachine::update_launchState()
{
    if (movementSinceEnteredState > minimumPeriod)
        transition_launchToSwing();
    if (!canSwing())
        return transition_launchToReady();
}

void StateMachine::transition_launchToReady()
{
    transition(SwingState_Ready);
}

void StateMachine::transition_launchToSwing()
{
    playSwish();
    transition(SwingState_Swing);

    // As a special case, update the new state immediately to allow
    // same-frame impacts.
    update_swingState();
}

void StateMachine::update_swingState()
{
    maxSwingVelocity = std::max(velocity, maxSwingVelocity);
    strength = std::min(1.f, (maxSwingVelocity - minVelocity) / maxVelocity);

    // When velocity falls below minimum, transition to register the miss
    if (!canSwing())
        return transition_swingingToImpact();
    // Call hit with simulated=true to check for hit without actually causing an impact
    if(ptr.getClass().hit(ptr, strength, swingType, true))
        return transition_swingingToImpact();
}

void StateMachine::transition_swingingToImpact()
{
    ptr.getClass().hit(ptr, strength, swingType, false);
    transition(SwingState_Impact);
}

void StateMachine::update_impactState()
{
    if (velocity < minVelocity)
        return transition_impactToCooldown();
}

void StateMachine::transition_impactToCooldown()
{
    transition(SwingState_Cooldown);
}

}}
