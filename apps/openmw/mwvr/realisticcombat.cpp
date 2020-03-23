#include "realisticcombat.hpp"
#include "../mwbase/environment.hpp"
#include "../mwbase/soundmanager.hpp"
#include "../mwmechanics/weapontype.hpp"
#include <iomanip>

namespace MWVR { namespace RealisticCombat {

static const char* stateToString(SwingState florida)
{
    switch (florida)
    {
    case SwingState_Idle:
        return "Idle";
    case SwingState_Impact:
        return "Impact";
    case SwingState_Ready:
        return "Ready";
    case SwingState_Swinging:
        return "Swinging";
    }
}

StateMachine::StateMachine(MWWorld::Ptr ptr) : ptr(ptr) {}

bool StateMachine::canSwing()
{
    if (velocity >= minVelocity)
        if(swingType != ESM::Weapon::AT_Thrust || thrustVelocity >= 0.f)
        return true;
    return false;
}

// Actions common to all transitions
void StateMachine::transition(
    SwingState newState)
{
    Log(Debug::Verbose) << "Transition: " << stateToString(state) << " -> " << stateToString(newState);

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

void StateMachine::update(float dt, bool enabled)
{
    auto* session = Environment::get().getSession();
    auto* world = MWBase::Environment::get().getWorld();
    auto& predictedPoses = session->predictedPoses(OpenXRSession::PredictionSlice::Predraw);
    auto& handPose = predictedPoses.hands[(int)MWVR::TrackedSpace::STAGE][(int)MWVR::Side::RIGHT_HAND];
    auto& headPose = predictedPoses.head[(int)MWVR::TrackedSpace::STAGE];
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

    // Chop is vertical, relative to the orientation of the weapon
    osg::Vec3 chopDirection = weaponDir * osg::Vec3{ 0,0,1 };

    // Swing is horizontal, relative to the orientation of the weapon
    osg::Vec3 slashDirection = weaponDir * osg::Vec3{ 1,0,0 };


    // Next determine current hand movement

    // If tracking is lost, openxr will return a position of 0
    // Reset position when tracking is re-acquired
    // Theoretically, the player's hand really could be at 0,0,0
    // but that's a super rare case so whatever.
    if (previousPosition == osg::Vec3(0.f, 0.f, 0.f))
        previousPosition = handPose.position;

    osg::Vec3 movement = handPose.position - previousPosition;
    movementSinceEnteredState += movement.length();
    previousPosition = handPose.position;
    osg::Vec3 swingVector = movement / dt;

    // Compute swing velocity
    // Unidirectional
    thrustVelocity = swingVector * thrustDirection;
    // Bidirectional
    slashVelocity = std::abs(swingVector * slashDirection);
    chopVelocity = std::abs(swingVector * chopDirection);
    velocity = swingVector.length();

    // Pick swing type based on greatest current velocity
    // Note i use abs() of thrust velocity to prevent accidentally triggering
    // chop or slash when player is withdrawing his limb.
    if (std::abs(thrustVelocity) > slashVelocity && std::abs(thrustVelocity) > chopVelocity)
    {
        swingType = ESM::Weapon::AT_Thrust;
    }
    else if (slashVelocity > chopVelocity)
    {
        swingType = ESM::Weapon::AT_Slash;
    }
    else
    {
        swingType = ESM::Weapon::AT_Chop;
    }

    switch (state)
    {
    case SwingState_Idle:
        return update_idleState();
    case SwingState_Ready:
        return update_readyState();
    case SwingState_Swinging:
        return update_swingingState();
    case SwingState_Impact:
        return update_impactState();
    }
}

void StateMachine::update_idleState()
{
    if (timeSinceEnteredState >= minimumPeriod)
        transition_idleToReady();
}

void StateMachine::transition_idleToReady()
{
    transition(SwingState_Ready);
}

void StateMachine::update_readyState()
{
    if (canSwing())
        return transition_readyToSwinging();
}

void StateMachine::transition_readyToSwinging()
{
    shouldSwish = true;
    transition(SwingState_Swinging);

    // As a special case, update the new state immediately to allow
    // same-frame impacts. This is important if the player is moving
    // at a velocity close to the minimum velocity.
    update_swingingState();
}

void StateMachine::playSwish()
{
    if (shouldSwish)
    {
        MWBase::SoundManager* sndMgr = MWBase::Environment::get().getSoundManager();

        std::string sound = "Weapon Swish";
        if (strength < 0.5f)
            sndMgr->playSound3D(ptr, sound, 1.0f, 0.8f); //Weak attack
        if (strength < 1.0f)
            sndMgr->playSound3D(ptr, sound, 1.0f, 1.0f); //Medium attack
        else
            sndMgr->playSound3D(ptr, sound, 1.0f, 1.2f); //Strong attack
        shouldSwish = false;
    }
}

void StateMachine::update_swingingState()
{
    maxSwingVelocity = std::max(velocity, maxSwingVelocity);
    strength = std::min(1.f, (maxSwingVelocity - minVelocity) / maxVelocity);

    // Require a minimum movement of the hand before a hit can be made
    // This is to prevent microswings
    if (movementSinceEnteredState > minimumPeriod)
    {
        playSwish();
        if (// When velocity falls below minimum, register the miss
            !canSwing() 
            // Call hit with simulated=true to check for hit
            || ptr.getClass().hit(ptr, strength, swingType, true)
            )
            return transition_swingingToImpact();
    }

    // If velocity drops below minimum before minimum movement was achieved,
    // drop back to ready
    if (!canSwing())
        return transition_swingingToReady();
}

void StateMachine::transition_swingingToReady()
{
    transition(SwingState_Ready);
}

void StateMachine::transition_swingingToImpact()
{
    ptr.getClass().hit(ptr, strength, swingType, false);
    transition(SwingState_Impact);
    std::cout << "Transition: Swing -> Impact" << std::endl;
}

void StateMachine::update_impactState()
{
    if (velocity < minVelocity)
        return transition_impactToIdle();
}

void StateMachine::transition_impactToIdle()
{
    transition(SwingState_Idle);
    std::cout << "Transition: Impact -> Ready" << std::endl;
}

}}
