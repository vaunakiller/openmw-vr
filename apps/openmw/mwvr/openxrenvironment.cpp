#include "openxrenvironment.hpp"

#include <cassert>

#include "openxranimation.hpp"
#include "openxrinputmanager.hpp"
#include "openxrsession.hpp"
#include "openxrmenu.hpp"

#include "../mwbase/environment.hpp"

MWVR::OpenXREnvironment *MWVR::OpenXREnvironment::sThis = 0;

MWVR::OpenXREnvironment::OpenXREnvironment()
: mSession(nullptr)
{
    assert (!sThis);
    sThis = this;
}

MWVR::OpenXREnvironment::~OpenXREnvironment()
{
    cleanup();
    sThis = 0;
}

void MWVR::OpenXREnvironment::cleanup()
{
    if (mSession)
        delete mSession;
    mSession = nullptr;
    if (mMenuManager)
        delete mMenuManager;
    mMenuManager = nullptr;
    if (mPlayerAnimation)
        delete mPlayerAnimation;
    mPlayerAnimation = nullptr;
    if (mViewer)
        delete mViewer;
    mViewer = nullptr;
    if (mOpenXRManager)
        delete mOpenXRManager;
    mOpenXRManager = nullptr;
}

MWVR::OpenXREnvironment& MWVR::OpenXREnvironment::get()
{
    assert (sThis);
    return *sThis;
}

MWVR::OpenXRInputManager* MWVR::OpenXREnvironment::getInputManager() const
{
    auto* inputManager = MWBase::Environment::get().getInputManager();
    assert(inputManager);
    auto xrInputManager = dynamic_cast<MWVR::OpenXRInputManager*>(inputManager);
    assert(xrInputManager);
    return xrInputManager;
}

MWVR::OpenXRSession* MWVR::OpenXREnvironment::getSession() const
{
    return mSession;
}

void MWVR::OpenXREnvironment::setSession(MWVR::OpenXRSession* xrSession)
{
    mSession = xrSession;
}

MWVR::OpenXRMenuManager* MWVR::OpenXREnvironment::getMenuManager() const
{
    return mMenuManager;
}

void MWVR::OpenXREnvironment::setMenuManager(MWVR::OpenXRMenuManager* menuManager)
{
    mMenuManager = menuManager;
}

MWVR::OpenXRAnimation* MWVR::OpenXREnvironment::getPlayerAnimation() const
{
    return mPlayerAnimation;
}

void MWVR::OpenXREnvironment::setPlayerAnimation(MWVR::OpenXRAnimation* xrAnimation)
{
    mPlayerAnimation = xrAnimation;
}


MWVR::OpenXRViewer* MWVR::OpenXREnvironment::getViewer() const
{
    return mViewer;
}

void MWVR::OpenXREnvironment::setViewer(MWVR::OpenXRViewer* xrViewer)
{
    mViewer = xrViewer;
}

MWVR::OpenXRManager* MWVR::OpenXREnvironment::getManager() const
{
    return mOpenXRManager;
}

void MWVR::OpenXREnvironment::setManager(MWVR::OpenXRManager* xrManager)
{
    mOpenXRManager = xrManager;
}

float MWVR::OpenXREnvironment::unitsPerMeter() const
{
    return mUnitsPerMeter;
}

void MWVR::OpenXREnvironment::setUnitsPerMeter(float unitsPerMeter)
{
    mUnitsPerMeter = unitsPerMeter;
}
