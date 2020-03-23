#include "vrenvironment.hpp"

#include <cassert>

#include "vranimation.hpp"
#include "openxrinputmanager.hpp"
#include "openxrsession.hpp"
#include "openxrmenu.hpp"

#include "../mwbase/environment.hpp"

MWVR::Environment *MWVR::Environment::sThis = 0;

MWVR::Environment::Environment()
: mSession(nullptr)
{
    assert (!sThis);
    sThis = this;
}

MWVR::Environment::~Environment()
{
    cleanup();
    sThis = 0;
}

void MWVR::Environment::cleanup()
{
    if (mSession)
        delete mSession;
    mSession = nullptr;
    if (mMenuManager)
        delete mMenuManager;
    mMenuManager = nullptr;
    if (mViewer)
        delete mViewer;
    mViewer = nullptr;
    if (mOpenXRManager)
        delete mOpenXRManager;
    mOpenXRManager = nullptr;
}

MWVR::Environment& MWVR::Environment::get()
{
    assert (sThis);
    return *sThis;
}

MWVR::OpenXRInputManager* MWVR::Environment::getInputManager() const
{
    auto* inputManager = MWBase::Environment::get().getInputManager();
    assert(inputManager);
    auto xrInputManager = dynamic_cast<MWVR::OpenXRInputManager*>(inputManager);
    assert(xrInputManager);
    return xrInputManager;
}

MWVR::OpenXRSession* MWVR::Environment::getSession() const
{
    return mSession;
}

void MWVR::Environment::setSession(MWVR::OpenXRSession* xrSession)
{
    mSession = xrSession;
}

MWVR::OpenXRMenuManager* MWVR::Environment::getMenuManager() const
{
    return mMenuManager;
}

void MWVR::Environment::setMenuManager(MWVR::OpenXRMenuManager* menuManager)
{
    mMenuManager = menuManager;
}

MWVR::VRAnimation* MWVR::Environment::getPlayerAnimation() const
{
    return mPlayerAnimation;
}

void MWVR::Environment::setPlayerAnimation(MWVR::VRAnimation* xrAnimation)
{
    mPlayerAnimation = xrAnimation;
}


MWVR::OpenXRViewer* MWVR::Environment::getViewer() const
{
    return mViewer;
}

void MWVR::Environment::setViewer(MWVR::OpenXRViewer* xrViewer)
{
    mViewer = xrViewer;
}

MWVR::OpenXRManager* MWVR::Environment::getManager() const
{
    return mOpenXRManager;
}

void MWVR::Environment::setManager(MWVR::OpenXRManager* xrManager)
{
    mOpenXRManager = xrManager;
}

float MWVR::Environment::unitsPerMeter() const
{
    return mUnitsPerMeter;
}

void MWVR::Environment::setUnitsPerMeter(float unitsPerMeter)
{
    mUnitsPerMeter = unitsPerMeter;
}
