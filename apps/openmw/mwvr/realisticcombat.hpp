#ifndef MWVR_REALISTICCOMBAT_H
#define MWVR_REALISTICCOMBAT_H

#include <components/esm/loadweap.hpp>

#include "../mwbase/world.hpp"
#include "../mwworld/ptr.hpp"
#include "../mwworld/class.hpp"

#include "vrenvironment.hpp"
#include "vrsession.hpp"

namespace MWVR { namespace RealisticCombat {
enum SwingState
{
    SwingState_Ready,
    SwingState_Launch,
    SwingState_Swing,
    SwingState_Impact,
    SwingState_Cooldown,
};

struct StateMachine
{
    // TODO: These should be configurable
    const float minVelocity = 1.f;
    const float maxVelocity = 4.f;

    float velocity = 0.f;
    float maxSwingVelocity = 0.f;

    SwingState state = SwingState_Ready;
    MWWorld::Ptr ptr = MWWorld::Ptr();
    int swingType = -1;
    float strength = 0.f;

    float thrustVelocity{ 0.f };
    float slashChopVelocity{ 0.f };
    float sideVelocity{ 0.f };

    float minimumPeriod{ .25f };

    float timeSinceEnteredState = { 0.f };
    float movementSinceEnteredState = { 0.f };

    bool mEnabled = false;

    osg::Vec3 previousPosition{ 0.f,0.f,0.f };

    StateMachine(MWWorld::Ptr ptr);

    bool canSwing();

    void playSwish();
    void reset();

    void transition(SwingState newState);

    void update(float dt, bool enabled);

    void update_cooldownState();
    void transition_cooldownToReady();

    void update_readyState();
    void transition_readyToLaunch();

    void update_launchState();
    void transition_launchToReady();
    void transition_launchToSwing();

    void update_swingState();
    void transition_swingingToImpact();

    void update_impactState();
    void transition_impactToCooldown();
};

}}

#endif