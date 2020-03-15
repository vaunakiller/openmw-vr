#include "realisticcombat.hpp"
#include "../mwbase/environment.hpp"
#include "../mwbase/soundmanager.hpp"

namespace MWVR { namespace RealisticCombat {

StateMachine::StateMachine(MWWorld::Ptr ptr) : ptr(ptr) {}

// Actions common to all transitions
void StateMachine::transition(
    SwingState newState)
{
    if (newState == state)
        throw std::logic_error("Cannot transition to current state");

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

void StateMachine::update(float dt, bool enabled)
{
    auto* session = Environment::get().getSession();
    auto& handPose = session->predictedPoses().hands[(int)MWVR::TrackedSpace::STAGE][(int)MWVR::Side::RIGHT_HAND];
    auto& headPose = session->predictedPoses().head[(int)MWVR::TrackedSpace::STAGE];

    if (mEnabled != enabled)
    {
        reset();
        mEnabled = enabled;
    }
    if (!enabled)
        return;

    timeSinceEnteredState += dt;


    // First determine direction of different swing types

    // Thrust is radially out from the head. Which is the same as the position of the hand relative to the head, ignoring height component
    osg::Vec3 thrustDirection = handPose.position - headPose.position;
    thrustDirection.z() = 0;
    thrustDirection.normalize();

    // Chop is straight down
    osg::Vec3 chopDirection = osg::Vec3(0.f, 0.f, -1.f);

    // Swing is normal to the plane created by Chop x Thrust
    osg::Vec3 slashDirection = chopDirection ^ thrustDirection;
    slashDirection.normalize();


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
    osg::Vec3 swingDirection = movement / dt;

    // Compute swing velocity
    // Unidirectional
    thrustVelocity = swingDirection * thrustDirection;
    // Bidirectional
    slashVelocity = std::abs(swingDirection * slashDirection);
    chopVelocity = std::abs(swingDirection * chopDirection);

    // Pick swing type based on greatest current velocity
    // Note i use abs() of thrust velocity to prevent accidentally triggering
    // chop or slash when player is withdrawing his limb.
    if (std::abs(thrustVelocity) > slashVelocity && std::abs(thrustVelocity) > chopVelocity)
    {
        velocity = thrustVelocity;
        swingType = ESM::Weapon::AT_Thrust;
    }
    else if (slashVelocity > chopVelocity)
    {
        velocity = slashVelocity;
        swingType = ESM::Weapon::AT_Slash;
    }
    else
    {
        velocity = chopVelocity;
        swingType = ESM::Weapon::AT_Chop;
    }


    switch (state)
    {
    case SwingState_Ready:
        return update_readyState();
    case SwingState_Swinging:
        return update_swingingState();
    case SwingState_Impact:
        return update_impactState();
    }
}

void StateMachine::update_readyState()
{
    if (velocity >= minVelocity)
        if(timeSinceEnteredState >= minimumPeriod)
            return transition_readyToSwinging();
}

void StateMachine::transition_readyToSwinging()
{
    shouldSwish = true;
    transition(SwingState_Swinging);

    // As an exception, update the new state immediately to allow
    // same-frame impacts.
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

    if (velocity < minVelocity)
        return transition_swingingToReady();

    // Require a minimum period of swinging before a hit can be made
    // This is to prevent annoying little microswings
    if (movementSinceEnteredState > minimumPeriod)
    {
        playSwish();

        // Note: calling hit with simulated=true to avoid side effects
        if (ptr.getClass().hit(ptr, strength, swingType, true))
            return transition_swingingToImpact();
    }
}

void StateMachine::transition_swingingToReady()
{
    if (movementSinceEnteredState > minimumPeriod)
    {
        playSwish();
        ptr.getClass().hit(ptr, strength, swingType, false);
    }
    transition(SwingState_Ready);
}

void StateMachine::transition_swingingToImpact()
{
    playSwish();
    ptr.getClass().hit(ptr, strength, swingType, false);
    transition(SwingState_Impact);
}

void StateMachine::update_impactState()
{
    if (velocity < minVelocity)
        return transition_impactToReady();
}

void StateMachine::transition_impactToReady()
{
    transition(SwingState_Ready);
}

}}
