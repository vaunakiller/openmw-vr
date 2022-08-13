#ifndef ENGINE_H
#define ENGINE_H

#include <components/compiler/extensions.hpp>
#include <components/files/collections.hpp>
#include <components/translation/translation.hpp>
#include <components/settings/settings.hpp>

#include <osgViewer/Viewer>
#include <osgViewer/ViewerEventHandlers>

#include "mwbase/environment.hpp"

#include "mwworld/ptr.hpp"

namespace Resource
{
    class ResourceSystem;
}

namespace SceneUtil
{
    class WorkQueue;
    class AsyncScreenCaptureOperation;
    class UnrefQueue;
}

namespace VFS
{
    class Manager;
}

namespace Compiler
{
    class Context;
}

namespace MWLua
{
    class LuaManager;
}

namespace Misc
{
    class CallbackManager;
}

namespace Stereo
{
    class Manager;
}

namespace Files
{
    struct ConfigurationManager;
}

namespace osgViewer
{
    class ScreenCaptureHandler;
}

namespace SceneUtil
{
    class SelectDepthFormatOperation;

    namespace Color
    {
        class SelectColorFormatOperation;
    }
}

namespace VR
{
    class Session;
    class TrackingManager;
    class Viewer;
}

namespace XR
{
    class Instance;
    class Session;
}

namespace MWVR
{
    class VRGUIManager;
}

namespace MWState
{
    class StateManager;
}

namespace MWGui
{
    class WindowManager;
}

namespace MWInput
{
    class InputManager;
}

namespace MWSound
{
    class SoundManager;
}

namespace MWWorld
{
    class World;
}

namespace MWScript
{
    class ScriptManager;
}

namespace MWMechanics
{
    class MechanicsManager;
}

namespace MWDialogue
{
    class DialogueManager;
}

namespace MWDialogue
{
    class Journal;
}

struct SDL_Window;

namespace OMW
{
    /// \brief Main engine class, that brings together all the components of OpenMW
    class Engine
    {
            SDL_Window* mWindow;
            std::unique_ptr<VFS::Manager> mVFS;
            std::unique_ptr<Resource::ResourceSystem> mResourceSystem;
            osg::ref_ptr<SceneUtil::WorkQueue> mWorkQueue;
            std::unique_ptr<SceneUtil::UnrefQueue> mUnrefQueue;
            std::unique_ptr<MWWorld::World> mWorld;
            std::unique_ptr<MWSound::SoundManager> mSoundManager;
            std::unique_ptr<MWScript::ScriptManager> mScriptManager;
            std::unique_ptr<MWGui::WindowManager> mWindowManager;
            std::unique_ptr<MWMechanics::MechanicsManager> mMechanicsManager;
            std::unique_ptr<MWDialogue::DialogueManager> mDialogueManager;
            std::unique_ptr<MWDialogue::Journal> mJournal;
            std::unique_ptr<MWInput::InputManager> mInputManager;
            std::unique_ptr<MWState::StateManager> mStateManager;
            std::unique_ptr<MWLua::LuaManager> mLuaManager;
            MWBase::Environment mEnvironment;
            ToUTF8::FromType mEncoding;
            std::unique_ptr<ToUTF8::Utf8Encoder> mEncoder;
            Files::PathContainer mDataDirs;
            std::vector<std::string> mArchives;
            boost::filesystem::path mResDir;
            osg::ref_ptr<osgViewer::Viewer> mViewer;
            osg::ref_ptr<osgViewer::ScreenCaptureHandler> mScreenCaptureHandler;
            osg::ref_ptr<SceneUtil::AsyncScreenCaptureOperation> mScreenCaptureOperation;
            osg::ref_ptr<SceneUtil::SelectDepthFormatOperation> mSelectDepthFormatOperation;
            osg::ref_ptr<SceneUtil::Color::SelectColorFormatOperation> mSelectColorFormatOperation;
            std::string mCellName;
            std::vector<std::string> mContentFiles;
            std::vector<std::string> mGroundcoverFiles;

            std::unique_ptr<Stereo::Manager> mStereoManager;
            std::unique_ptr<Misc::CallbackManager> mCallbackManager;

            bool mSkipMenu;
            bool mUseSound;
            bool mCompileAll;
            bool mCompileAllDialogue;
            int mWarningsMode;
            std::string mFocusName;
            bool mScriptConsoleMode;
            std::string mStartupScript;
            int mActivationDistanceOverride;
            std::string mSaveGameFile;
            // Grab mouse?
            bool mGrab;

            unsigned int mRandomSeed;

            Compiler::Extensions mExtensions;
            std::unique_ptr<Compiler::Context> mScriptContext;

            Files::Collections mFileCollections;
            bool mFSStrict;
            Translation::Storage mTranslationDataStorage;
            std::vector<std::string> mScriptBlacklist;
            bool mScriptBlacklistUse;
            bool mNewGame;

            // not implemented
            Engine (const Engine&);
            Engine& operator= (const Engine&);

            void executeLocalScripts();

            bool frame (float dt);

            /// Prepare engine for game play
            void prepareEngine();

            void createWindow();
            void setWindowIcon();

        public:
            Engine(Files::ConfigurationManager& configurationManager);
            virtual ~Engine();

            /// Enable strict filesystem mode (do not fold case)
            ///
            /// \attention The strict mode must be specified before any path-related settings
            /// are given to the engine.
            void enableFSStrict(bool fsStrict);

            /// Set data dirs
            void setDataDirs(const Files::PathContainer& dataDirs);

            /// Add BSA archive
            void addArchive(const std::string& archive);

            /// Set resource dir
            void setResourceDir(const boost::filesystem::path& parResDir);

            /// Set start cell name
            void setCell(const std::string& cellName);

            /**
             * @brief addContentFile - Adds content file (ie. esm/esp, or omwgame/omwaddon) to the content files container.
             * @param file - filename (extension is required)
             */
            void addContentFile(const std::string& file);
            void addGroundcoverFile(const std::string& file);

            /// Disable or enable all sounds
            void setSoundUsage(bool soundUsage);

            /// Skip main menu and go directly into the game
            ///
            /// \param newGame Start a new game instead off dumping the player into the game
            /// (ignored if !skipMenu).
            void setSkipMenu (bool skipMenu, bool newGame);

            void setGrabMouse(bool grab) { mGrab = grab; }

            /// Initialise and enter main loop.
            void go();

            /// Compile all scripts (excludign dialogue scripts) at startup?
            void setCompileAll (bool all);

            /// Compile all dialogue scripts at startup?
            void setCompileAllDialogue (bool all);

            /// Font encoding
            void setEncoding(const ToUTF8::FromType& encoding);

            /// Enable console-only script functionality
            void setScriptConsoleMode (bool enabled);

            /// Set path for a script that is run on startup in the console.
            void setStartupScript (const std::string& path);

            /// Override the game setting specified activation distance.
            void setActivationDistanceOverride (int distance);

            void setWarningsMode (int mode);

            void setScriptBlacklist (const std::vector<std::string>& list);

            void setScriptBlacklistUse (bool use);

            /// Set the save game file to load after initialising the engine.
            void setSaveGameFile(const std::string& savegame);

            void setRandomSeed(unsigned int seed);

            void configureVR(osg::GraphicsContext* gc);

        private:
            Files::ConfigurationManager& mCfgMgr;
            class LuaWorker;
            int mGlMaxTextureImageUnits;

#ifdef USE_OPENXR
            std::unique_ptr<VR::TrackingManager> mVrTrackingManager;
            std::unique_ptr<MWVR::VRGUIManager> mVrGUIManager;
            std::unique_ptr<XR::Instance> mXrInstance;
            std::shared_ptr<XR::Session> mXrSession;
            std::unique_ptr<VR::Viewer> mVrViewer;
#endif
    };
}

#endif /* ENGINE_H */
