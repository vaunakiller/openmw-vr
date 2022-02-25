#ifndef GAME_BASE_ENVIRONMENT_H
#define GAME_BASE_ENVIRONMENT_H

namespace osg
{
    class Stats;
}

namespace Resource
{
    class ResourceSystem;
}

namespace MWBase
{
    class World;
    class ScriptManager;
    class DialogueManager;
    class Journal;
    class SoundManager;
    class MechanicsManager;
    class InputManager;
    class WindowManager;
    class StateManager;
    class LuaManager;

    /// \brief Central hub for mw-subsystems
    ///
    /// This class allows each mw-subsystem to access any others subsystem's top-level manager class.
    ///
    /// \attention Environment takes ownership of the manager class instances it is handed over in
    /// the set* functions.
    class Environment
    {
            static Environment *sThis;

            World *mWorld;
            SoundManager *mSoundManager;
            ScriptManager *mScriptManager;
            WindowManager *mWindowManager;
            MechanicsManager *mMechanicsManager;
            DialogueManager *mDialogueManager;
            Journal *mJournal;
            InputManager *mInputManager;
            StateManager *mStateManager;
            LuaManager *mLuaManager;
            Resource::ResourceSystem *mResourceSystem;
            float mFrameDuration;
            float mFrameRateLimit;

            Environment (const Environment&);
            ///< not implemented

            Environment& operator= (const Environment&);
            ///< not implemented

        public:

            Environment();

            ~Environment();

            void setWorld (World *world);

            void setSoundManager (SoundManager *soundManager);

            void setScriptManager (MWBase::ScriptManager *scriptManager);

            void setWindowManager (WindowManager *windowManager);

            void setMechanicsManager (MechanicsManager *mechanicsManager);

            void setDialogueManager (DialogueManager *dialogueManager);

            void setJournal (Journal *journal);

            void setInputManager (InputManager *inputManager);

            void setStateManager (StateManager *stateManager);

            void setLuaManager (LuaManager *luaManager);

            void setResourceSystem (Resource::ResourceSystem *resourceSystem);

            void setFrameDuration (float duration);
            ///< Set length of current frame in seconds.

            void setFrameRateLimit(float frameRateLimit);
            float getFrameRateLimit() const;

            World *getWorld() const;

            SoundManager *getSoundManager() const;

            ScriptManager *getScriptManager() const;

            WindowManager *getWindowManager() const;

            MechanicsManager *getMechanicsManager() const;

            DialogueManager *getDialogueManager() const;

            Journal *getJournal() const;

            InputManager *getInputManager() const;

            StateManager *getStateManager() const;

            LuaManager *getLuaManager() const;

            Resource::ResourceSystem *getResourceSystem() const;

            float getFrameDuration() const;

            void cleanup();
            ///< Delete all mw*-subsystems.

            static const Environment& get();
            ///< Return instance of this class.

            void reportStats(unsigned int frameNumber, osg::Stats& stats) const;
    };
}

#endif
