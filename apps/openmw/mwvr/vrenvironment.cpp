#include "vrenvironment.hpp"

#include <cassert>

#include "vranimation.hpp"
#include "vrinputmanager.hpp"
#include "vrgui.hpp"
#include "vrviewer.hpp"

#include "../mwbase/environment.hpp"

MWVR::Environment* MWVR::Environment::sThis = 0;

MWVR::Environment::Environment()
{
    assert(!sThis);
    sThis = this;
}

MWVR::Environment::~Environment()
{
    cleanup();
    sThis = 0;
}

void MWVR::Environment::cleanup()
{
    if (mGUIManager)
        delete mGUIManager;
    mGUIManager = nullptr;
    if (mViewer)
        delete mViewer;
    mViewer = nullptr;
}

MWVR::Environment& MWVR::Environment::get()
{
    assert(sThis);
    return *sThis;
}

MWVR::VRInputManager* MWVR::Environment::getInputManager() const
{
    auto* inputManager = MWBase::Environment::get().getInputManager();
    assert(inputManager);
    auto xrInputManager = dynamic_cast<MWVR::VRInputManager*>(inputManager);
    assert(xrInputManager);
    return xrInputManager;
}

MWVR::VRGUIManager* MWVR::Environment::getGUIManager() const
{
    return mGUIManager;
}

void MWVR::Environment::setGUIManager(MWVR::VRGUIManager* GUIManager)
{
    mGUIManager = GUIManager;
}

MWVR::VRAnimation* MWVR::Environment::getPlayerAnimation() const
{
    return mPlayerAnimation;
}

void MWVR::Environment::setPlayerAnimation(MWVR::VRAnimation* xrAnimation)
{
    mPlayerAnimation = xrAnimation;
}


MWVR::VRViewer* MWVR::Environment::getViewer() const
{
    return mViewer;
}

void MWVR::Environment::setViewer(MWVR::VRViewer* xrViewer)
{
    mViewer = xrViewer;
}
