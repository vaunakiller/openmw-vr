#ifndef MWVR_REALISTICCOMBAT_H
#define MWVR_REALISTICCOMBAT_H

#include <components/esm/loadweap.hpp>

#include "../mwbase/world.hpp"
#include "../mwworld/ptr.hpp"
#include "../mwworld/class.hpp"

#include "vrenvironment.hpp"
#include "openxrsession.hpp"

namespace MWVR { namespace RealisticCombat {
enum SwingState
{
    SwingState_Ready = 0,
    SwingState_Swinging = 1,
    SwingState_Impact = 2
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
    float slashVelocity{ 0.f };
    float chopVelocity{ 0.f };

    float minimumPeriod{ .5f };
    float timeSinceEnteredState = { 0.f };
    float movementSinceEnteredState = { 0.f };

    bool shouldSwish = false;
    bool mEnabled = false;

    osg::Vec3 previousPosition{ 0.f,0.f,0.f };

    StateMachine(MWWorld::Ptr ptr);

    void playSwish();
    void reset();

    void transition(SwingState newState);

    void update(float dt, bool enabled);

    void update_readyState();
    void transition_readyToSwinging();

    void update_swingingState();
    void transition_swingingToReady();
    void transition_swingingToImpact();

    void update_impactState();
    void transition_impactToReady();
};

}}

#endif
