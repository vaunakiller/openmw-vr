#include "environment.hpp"

#include <cassert>

#include <components/resource/resourcesystem.hpp>

#include "world.hpp"
#include "scriptmanager.hpp"
#include "dialoguemanager.hpp"
#include "journal.hpp"
#include "soundmanager.hpp"
#include "mechanicsmanager.hpp"
#include "inputmanager.hpp"
#include "windowmanager.hpp"
#include "statemanager.hpp"
#include "luamanager.hpp"

MWBase::Environment *MWBase::Environment::sThis = nullptr;

MWBase::Environment::Environment()
: mWorld (nullptr), mSoundManager (nullptr), mScriptManager (nullptr), mWindowManager (nullptr),
  mMechanicsManager (nullptr),  mDialogueManager (nullptr), mJournal (nullptr), mInputManager (nullptr),
    mStateManager (nullptr), mLuaManager (nullptr), mResourceSystem (nullptr),  mFrameDuration (0), mFrameRateLimit(0.f)
{
    assert (!sThis);
    sThis = this;
}

MWBase::Environment::~Environment()
{
    cleanup();
    sThis = nullptr;
}

void MWBase::Environment::setWorld (World *world)
{
    mWorld = world;
}

void MWBase::Environment::setSoundManager (SoundManager *soundManager)
{
    mSoundManager = soundManager;
}

void MWBase::Environment::setScriptManager (ScriptManager *scriptManager)
{
    mScriptManager = scriptManager;
}

void MWBase::Environment::setWindowManager (WindowManager *windowManager)
{
    mWindowManager = windowManager;
}

void MWBase::Environment::setMechanicsManager (MechanicsManager *mechanicsManager)
{
    mMechanicsManager = mechanicsManager;
}

void MWBase::Environment::setDialogueManager (DialogueManager *dialogueManager)
{
    mDialogueManager = dialogueManager;
}

void MWBase::Environment::setJournal (Journal *journal)
{
    mJournal = journal;
}

void MWBase::Environment::setInputManager (InputManager *inputManager)
{
    mInputManager = inputManager;
}

void MWBase::Environment::setStateManager (StateManager *stateManager)
{
    mStateManager = stateManager;
}

void MWBase::Environment::setLuaManager (LuaManager *luaManager)
{
    mLuaManager = luaManager;
}

void MWBase::Environment::setResourceSystem (Resource::ResourceSystem *resourceSystem)
{
    mResourceSystem = resourceSystem;
}

void MWBase::Environment::setFrameDuration (float duration)
{
    mFrameDuration = duration;
}

void MWBase::Environment::setFrameRateLimit(float limit)
{
    mFrameRateLimit = limit;
}

float MWBase::Environment::getFrameRateLimit() const
{
    return mFrameRateLimit;
}

MWBase::World *MWBase::Environment::getWorld() const
{
    assert (mWorld);
    return mWorld;
}

MWBase::SoundManager *MWBase::Environment::getSoundManager() const
{
    assert (mSoundManager);
    return mSoundManager;
}

MWBase::ScriptManager *MWBase::Environment::getScriptManager() const
{
    assert (mScriptManager);
    return mScriptManager;
}

MWBase::WindowManager *MWBase::Environment::getWindowManager() const
{
    assert (mWindowManager);
    return mWindowManager;
}

MWBase::MechanicsManager *MWBase::Environment::getMechanicsManager() const
{
    assert (mMechanicsManager);
    return mMechanicsManager;
}

MWBase::DialogueManager *MWBase::Environment::getDialogueManager() const
{
    assert (mDialogueManager);
    return mDialogueManager;
}

MWBase::Journal *MWBase::Environment::getJournal() const
{
    assert (mJournal);
    return mJournal;
}

MWBase::InputManager *MWBase::Environment::getInputManager() const
{
    assert (mInputManager);
    return mInputManager;
}

MWBase::StateManager *MWBase::Environment::getStateManager() const
{
    assert (mStateManager);
    return mStateManager;
}

MWBase::LuaManager *MWBase::Environment::getLuaManager() const
{
    assert (mLuaManager);
    return mLuaManager;
}

Resource::ResourceSystem *MWBase::Environment::getResourceSystem() const
{
    return mResourceSystem;
}

float MWBase::Environment::getFrameDuration() const
{
    return mFrameDuration;
}

void MWBase::Environment::cleanup()
{
    delete mMechanicsManager;
    mMechanicsManager = nullptr;

    delete mDialogueManager;
    mDialogueManager = nullptr;

    delete mJournal;
    mJournal = nullptr;

    delete mScriptManager;
    mScriptManager = nullptr;

    delete mWindowManager;
    mWindowManager = nullptr;

    delete mWorld;
    mWorld = nullptr;

    delete mSoundManager;
    mSoundManager = nullptr;

    delete mInputManager;
    mInputManager = nullptr;

    delete mStateManager;
    mStateManager = nullptr;

    delete mLuaManager;
    mLuaManager = nullptr;
}

const MWBase::Environment& MWBase::Environment::get()
{
    assert (sThis);
    return *sThis;
}

void MWBase::Environment::reportStats(unsigned int frameNumber, osg::Stats& stats) const
{
    mMechanicsManager->reportStats(frameNumber, stats);
    mWorld->reportStats(frameNumber, stats);
}
