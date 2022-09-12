#include "engine.hpp"

#include <iomanip>
#include <chrono>
#include <thread>
#include <filesystem>

#include <boost/filesystem/fstream.hpp>

#include <osgViewer/ViewerEventHandlers>
#include <osgDB/WriteFile>

#include <SDL.h>

#include <components/debug/debuglog.hpp>
#include <components/debug/gldebug.hpp>

#include <components/misc/rng.hpp>

#include <components/vfs/manager.hpp>
#include <components/vfs/registerarchives.hpp>

#include <components/sdlutil/sdlgraphicswindow.hpp>
#include <components/sdlutil/imagetosurface.hpp>

#include <components/shader/shadermanager.hpp>

#include <components/resource/resourcesystem.hpp>
#include <components/resource/scenemanager.hpp>
#include <components/resource/stats.hpp>

#include <components/compiler/extensions0.hpp>

#include <components/stereo/stereomanager.hpp>
#include <components/stereo/multiview.hpp>

#include <components/misc/callbackmanager.hpp>

#include <components/sceneutil/workqueue.hpp>

#include <components/files/configurationmanager.hpp>

#include <components/version/version.hpp>

#include <components/misc/frameratelimiter.hpp>

#include <components/sceneutil/screencapture.hpp>
#include <components/sceneutil/depth.hpp>
#include <components/sceneutil/color.hpp>
#include <components/sceneutil/util.hpp>
#include <components/sceneutil/unrefqueue.hpp>

#include <components/settings/shadermanager.hpp>

#include <components/vr/session.hpp>
#include <components/vr/trackingmanager.hpp>
#include <components/vr/viewer.hpp>
#include <components/vr/vr.hpp>
#include <components/xr/instance.hpp>
#include <components/xr/session.hpp>

#include "mwinput/inputmanagerimp.hpp"

#include "mwgui/windowmanagerimp.hpp"

#include "mwlua/luamanagerimp.hpp"

#include "mwscript/scriptmanagerimp.hpp"
#include "mwscript/interpretercontext.hpp"

#include "mwsound/soundmanagerimp.hpp"

#include "mwworld/class.hpp"
#include "mwworld/player.hpp"
#include "mwworld/worldimp.hpp"

#include "mwrender/vismask.hpp"
#include "mwrender/camera.hpp"

#include "mwclass/classes.hpp"

#include "mwdialogue/dialoguemanagerimp.hpp"
#include "mwdialogue/journalimp.hpp"
#include "mwdialogue/scripttest.hpp"

#include "mwmechanics/mechanicsmanagerimp.hpp"

#include "mwstate/statemanagerimp.hpp"

#ifdef USE_OPENXR
#include "mwvr/vrinputmanager.hpp"
#include "mwvr/vrgui.hpp"
#endif

namespace
{
    void checkSDLError(int ret)
    {
        if (ret != 0)
            Log(Debug::Error) << "SDL error: " << SDL_GetError();
    }

    struct UserStats
    {
        const std::string mLabel;
        const std::string mBegin;
        const std::string mEnd;
        const std::string mTaken;

        UserStats(const std::string& label, const std::string& prefix)
            : mLabel(label),
              mBegin(prefix + "_time_begin"),
              mEnd(prefix + "_time_end"),
              mTaken(prefix + "_time_taken")
        {}
    };

    enum class UserStatsType : std::size_t
    {
        Input,
        Sound,
        State,
        Script,
        Mechanics,
        Physics,
        PhysicsWorker,
        World,
        Gui,
        Lua,
        LuaSyncUpdate,
        Number,
    };

    template <UserStatsType type>
    struct UserStatsValue
    {
        static const UserStats sValue;
    };

    template <>
    const UserStats UserStatsValue<UserStatsType::Input>::sValue {"Input", "input"};

    template <>
    const UserStats UserStatsValue<UserStatsType::Sound>::sValue {"Sound", "sound"};

    template <>
    const UserStats UserStatsValue<UserStatsType::State>::sValue {"State", "state"};

    template <>
    const UserStats UserStatsValue<UserStatsType::Script>::sValue {"Script", "script"};

    template <>
    const UserStats UserStatsValue<UserStatsType::Mechanics>::sValue {"Mech", "mechanics"};

    template <>
    const UserStats UserStatsValue<UserStatsType::Physics>::sValue {"Phys", "physics"};

    template <>
    const UserStats UserStatsValue<UserStatsType::PhysicsWorker>::sValue {" -Async", "physicsworker"};

    template <>
    const UserStats UserStatsValue<UserStatsType::World>::sValue {"World", "world"};

    template <>
    const UserStats UserStatsValue<UserStatsType::Gui>::sValue {"Gui", "gui"};

    template <>
    const UserStats UserStatsValue<UserStatsType::Lua>::sValue {"Lua", "lua"};

    template <>
    const UserStats UserStatsValue<UserStatsType::LuaSyncUpdate>::sValue{ " -Sync", "luasyncupdate" };


    template <UserStatsType type>
    struct ForEachUserStatsValue
    {
        template <class F>
        static void apply(F&& f)
        {
            f(UserStatsValue<type>::sValue);
            using Next = ForEachUserStatsValue<static_cast<UserStatsType>(static_cast<std::size_t>(type) + 1)>;
            Next::apply(std::forward<F>(f));
        }
    };

    template <>
    struct ForEachUserStatsValue<UserStatsType::Number>
    {
        template <class F>
        static void apply(F&&) {}
    };

    template <class F>
    void forEachUserStatsValue(F&& f)
    {
        ForEachUserStatsValue<static_cast<UserStatsType>(0)>::apply(std::forward<F>(f));
    }

    template <UserStatsType sType>
    class ScopedProfile
    {
        public:
            ScopedProfile(osg::Timer_t frameStart, unsigned int frameNumber, const osg::Timer& timer, osg::Stats& stats)
                : mScopeStart(timer.tick()),
                  mFrameStart(frameStart),
                  mFrameNumber(frameNumber),
                  mTimer(timer),
                  mStats(stats)
            {
            }

            ScopedProfile(const ScopedProfile&) = delete;
            ScopedProfile& operator=(const ScopedProfile&) = delete;

            ~ScopedProfile()
            {
                if (!mStats.collectStats("engine"))
                    return;
                const osg::Timer_t end = mTimer.tick();
                const UserStats& stats = UserStatsValue<sType>::sValue;

                mStats.setAttribute(mFrameNumber, stats.mBegin, mTimer.delta_s(mFrameStart, mScopeStart));
                mStats.setAttribute(mFrameNumber, stats.mTaken, mTimer.delta_s(mScopeStart, end));
                mStats.setAttribute(mFrameNumber, stats.mEnd, mTimer.delta_s(mFrameStart, end));
            }

        private:
            const osg::Timer_t mScopeStart;
            const osg::Timer_t mFrameStart;
            const unsigned int mFrameNumber;
            const osg::Timer& mTimer;
            osg::Stats& mStats;
    };

    void initStatsHandler(Resource::Profiler& profiler)
    {
        const osg::Vec4f textColor(1.f, 1.f, 1.f, 1.f);
        const osg::Vec4f barColor(1.f, 1.f, 1.f, 1.f);
        const float multiplier = 1000;
        const bool average = true;
        const bool averageInInverseSpace = false;
        const float maxValue = 10000;

        forEachUserStatsValue([&] (const UserStats& v)
        {
            profiler.addUserStatsLine(v.mLabel, textColor, barColor, v.mTaken, multiplier,
                                      average, averageInInverseSpace, v.mBegin, v.mEnd, maxValue);
        });
        // the forEachUserStatsValue loop is "run" at compile time, hence the settings manager is not available.
        // Unconditionnally add the async physics stats, and then remove it at runtime if necessary
        if (Settings::Manager::getInt("async num threads", "Physics") == 0)
            profiler.removeUserStatsLine(" -Async");
    }

    struct ScheduleNonDialogMessageBox
    {
        void operator()(std::string message) const
        {
            MWBase::Environment::get().getWindowManager()->scheduleMessageBox(std::move(message), MWGui::ShowInDialogueMode_Never);
        }
    };

    struct IgnoreString
    {
        void operator()(std::string) const {}
    };

    class IdentifyOpenGLOperation : public osg::GraphicsOperation
    {
    public:
        IdentifyOpenGLOperation() : GraphicsOperation("IdentifyOpenGLOperation", false)
        {}

        void operator()(osg::GraphicsContext* graphicsContext) override
        {
            Log(Debug::Info) << "OpenGL Vendor: " << glGetString(GL_VENDOR);
            Log(Debug::Info) << "OpenGL Renderer: " << glGetString(GL_RENDERER);
            Log(Debug::Info) << "OpenGL Version: " << glGetString(GL_VERSION);
            glGetIntegerv(GL_MAX_TEXTURE_IMAGE_UNITS, &mMaxTextureImageUnits);
        }

        int getMaxTextureImageUnits() const
        {
            if (mMaxTextureImageUnits == 0)
                throw std::logic_error("mMaxTextureImageUnits is not initialized");
            return mMaxTextureImageUnits;
        }

    private:
        int mMaxTextureImageUnits = 0;
    };

    class InitializeStereoOperation final : public osg::GraphicsOperation
    {
    public:
        InitializeStereoOperation() : GraphicsOperation("InitializeStereoOperation", false)
        {}

        void operator()(osg::GraphicsContext* graphicsContext) override
        {
            Stereo::Manager::instance().initializeStereo(graphicsContext);
        }
    };

    class InitializeVrOperation : public osg::GraphicsOperation
    {
    public:
        InitializeVrOperation(OMW::Engine* engine) : GraphicsOperation("InitializeVrOperation", false)
            , mEngine(engine)
        {}

        void operator()(osg::GraphicsContext* graphicsContext) override
        {
            mEngine->configureVR(graphicsContext);
        }

        OMW::Engine* mEngine;
    };
}

void OMW::Engine::executeLocalScripts()
{
    MWWorld::LocalScripts& localScripts = mWorld->getLocalScripts();

    localScripts.startIteration();
    std::pair<std::string, MWWorld::Ptr> script;
    while (localScripts.getNext(script))
    {
        MWScript::InterpreterContext interpreterContext (
            &script.second.getRefData().getLocals(), script.second);
        mScriptManager->run (script.first, interpreterContext);
    }
}

bool OMW::Engine::frame(float frametime)
{
    try
    {
        const osg::Timer_t frameStart = mViewer->getStartTick();
        const unsigned int frameNumber = mViewer->getFrameStamp()->getFrameNumber();
        const osg::Timer* const timer = osg::Timer::instance();
        osg::Stats* const stats = mViewer->getViewerStats();

        mEnvironment.setFrameDuration(frametime);

        // update input
        {
            ScopedProfile<UserStatsType::Input> profile(frameStart, frameNumber, *timer, *stats);
            mInputManager->update(frametime, false);
        }

        // When the window is minimized, pause the game. Currently this *has* to be here to work around a MyGUI bug.
        // If we are not currently rendering, then RenderItems will not be reused resulting in a memory leak upon changing widget textures (fixed in MyGUI 3.3.2),
        // and destroyed widgets will not be deleted (not fixed yet, https://github.com/MyGUI/mygui/issues/21)
        {
            ScopedProfile<UserStatsType::Sound> profile(frameStart, frameNumber, *timer, *stats);

            if (!mWindowManager->isWindowVisible())
            {
                mSoundManager->pausePlayback();
                return false;
            }
            else
                mSoundManager->resumePlayback();

            // sound
            if (mUseSound)
                mSoundManager->update(frametime);
        }

        // Main menu opened? Then scripts are also paused.
        bool paused = mWindowManager->containsMode(MWGui::GM_MainMenu);
        
        {
            ScopedProfile<UserStatsType::LuaSyncUpdate> profile(frameStart, frameNumber, *timer, *stats);
            // Should be called after input manager update and before any change to the game world.
            // It applies to the game world queued changes from the previous frame.
            mLuaManager->synchronizedUpdate();
        }

        // update game state
        {
            ScopedProfile<UserStatsType::State> profile(frameStart, frameNumber, *timer, *stats);
            mStateManager->update (frametime);
        }

        bool guiActive = mWindowManager->isGuiMode();

        {
            ScopedProfile<UserStatsType::Script> profile(frameStart, frameNumber, *timer, *stats);

            if (mStateManager->getState() != MWBase::StateManager::State_NoGame)
            {
                if (!paused)
                {
                    if (mWorld->getScriptsEnabled())
                    {
                        // local scripts
                        executeLocalScripts();

                        // global scripts
                        mScriptManager->getGlobalScripts().run();
                    }

                    mWorld->markCellAsUnchanged();
                }

                if (!guiActive)
                {
                    double hours = (frametime * mWorld->getTimeScaleFactor()) / 3600.0;
                    mWorld->advanceTime(hours, true);
                    mWorld->rechargeItems(frametime, true);
                }
            }
        }

        // update mechanics
        {
            ScopedProfile<UserStatsType::Mechanics> profile(frameStart, frameNumber, *timer, *stats);

            if (mStateManager->getState() != MWBase::StateManager::State_NoGame)
            {
                mMechanicsManager->update(frametime, guiActive);
            }

            if (mStateManager->getState() == MWBase::StateManager::State_Running)
            {
                MWWorld::Ptr player = mWorld->getPlayerPtr();
                if(!guiActive && player.getClass().getCreatureStats(player).isDead())
                    mStateManager->endGame();
            }
        }

        // update physics
        {
            ScopedProfile<UserStatsType::Physics> profile(frameStart, frameNumber, *timer, *stats);

            if (mStateManager->getState() != MWBase::StateManager::State_NoGame)
            {
                mWorld->updatePhysics(frametime, guiActive, frameStart, frameNumber, *stats);
            }
        }

        // update world
        {
            ScopedProfile<UserStatsType::World> profile(frameStart, frameNumber, *timer, *stats);

            if (mStateManager->getState() != MWBase::StateManager::State_NoGame)
            {
                mWorld->update(frametime, guiActive);
            }
        }

        // update GUI
        {
            ScopedProfile<UserStatsType::Gui> profile(frameStart, frameNumber, *timer, *stats);
            mWindowManager->update(frametime);
        }

        const bool reportResource = stats->collectStats("resource");

        if (reportResource)
            stats->setAttribute(frameNumber, "UnrefQueue", mUnrefQueue->getSize());

        mUnrefQueue->flush(*mWorkQueue);

        if (reportResource)
        {
            stats->setAttribute(frameNumber, "FrameNumber", frameNumber);

            mResourceSystem->reportStats(frameNumber, stats);

            stats->setAttribute(frameNumber, "WorkQueue", mWorkQueue->getNumItems());
            stats->setAttribute(frameNumber, "WorkThread", mWorkQueue->getNumActiveThreads());

            mEnvironment.reportStats(frameNumber, *stats);
        }
    }
    catch (const std::exception& e)
    {
        Log(Debug::Error) << "Error in frame: " << e.what();
    }
    return true;
}

OMW::Engine::Engine(Files::ConfigurationManager& configurationManager)
  : mWindow(nullptr)
  , mEncoding(ToUTF8::WINDOWS_1252)
  , mScreenCaptureOperation(nullptr)
  , mSelectDepthFormatOperation(new SceneUtil::SelectDepthFormatOperation())
  , mSelectColorFormatOperation(new SceneUtil::Color::SelectColorFormatOperation())
  , mStereoManager(nullptr)
  , mSkipMenu (false)
  , mUseSound (true)
  , mCompileAll (false)
  , mCompileAllDialogue (false)
  , mWarningsMode (1)
  , mScriptConsoleMode (false)
  , mActivationDistanceOverride(-1)
  , mGrab(true)
  , mRandomSeed(0)
  , mFSStrict (false)
  , mScriptBlacklistUse (true)
  , mNewGame (false)
  , mCfgMgr(configurationManager)
  , mGlMaxTextureImageUnits(0)
#ifdef USE_OPENXR
  , mVrTrackingManager(nullptr)
  , mVrGUIManager(nullptr)
  , mXrInstance(nullptr)
#endif
{
    SDL_SetHint(SDL_HINT_ACCELEROMETER_AS_JOYSTICK, "0"); // We use only gamepads

    Uint32 flags = SDL_INIT_VIDEO|SDL_INIT_NOPARACHUTE|SDL_INIT_GAMECONTROLLER|SDL_INIT_JOYSTICK|SDL_INIT_SENSOR;
    if(SDL_WasInit(flags) == 0)
    {
        SDL_SetMainReady();
        if(SDL_Init(flags) != 0)
        {
            throw std::runtime_error("Could not initialize SDL! " + std::string(SDL_GetError()));
        }
    }
}

OMW::Engine::~Engine()
{
    if (mScreenCaptureOperation != nullptr)
        mScreenCaptureOperation->stop();

    mMechanicsManager = nullptr;
    mDialogueManager = nullptr;
    mJournal = nullptr;
    mScriptManager = nullptr;
    mWindowManager = nullptr;
    mWorld = nullptr;
    mSoundManager = nullptr;
    mInputManager = nullptr;
    mStateManager = nullptr;
    mLuaManager = nullptr;

    mScriptContext = nullptr;

    mUnrefQueue = nullptr;
    mWorkQueue = nullptr;


    mViewer = nullptr;
#ifdef USE_OPENXR
    mVrViewer = nullptr;
    mCallbackManager = nullptr;
    mStereoManager = nullptr;
    mVrGUIManager = nullptr;
    mXrSession = nullptr;
    mXrInstance = nullptr;
    mVrTrackingManager = nullptr;
#endif
    mResourceSystem.reset();

    mEncoder = nullptr;

    if (mWindow)
    {
        SDL_DestroyWindow(mWindow);
        mWindow = nullptr;
    }

    SDL_Quit();
}

void OMW::Engine::enableFSStrict(bool fsStrict)
{
    mFSStrict = fsStrict;
}

// Set data dir

void OMW::Engine::setDataDirs (const Files::PathContainer& dataDirs)
{
    mDataDirs = dataDirs;
    mDataDirs.insert(mDataDirs.begin(), (mResDir / "vfs"));
    mFileCollections = Files::Collections (mDataDirs, !mFSStrict);
}

// Add BSA archive
void OMW::Engine::addArchive (const std::string& archive) {
    mArchives.push_back(archive);
}

// Set resource dir
void OMW::Engine::setResourceDir (const boost::filesystem::path& parResDir)
{
    mResDir = parResDir;
}

// Set start cell name
void OMW::Engine::setCell (const std::string& cellName)
{
    mCellName = cellName;
}

void OMW::Engine::addContentFile(const std::string& file)
{
    mContentFiles.push_back(file);
}

void OMW::Engine::addGroundcoverFile(const std::string& file)
{
    mGroundcoverFiles.emplace_back(file);
}

void OMW::Engine::setSkipMenu (bool skipMenu, bool newGame)
{
    mSkipMenu = skipMenu;
    mNewGame = newGame;
}

void OMW::Engine::createWindow()
{
    int screen = Settings::Manager::getInt("screen", "Video");
    int width = Settings::Manager::getInt("resolution x", "Video");
    int height = Settings::Manager::getInt("resolution y", "Video");
    Settings::WindowMode windowMode = static_cast<Settings::WindowMode>(Settings::Manager::getInt("window mode", "Video"));
    bool windowBorder = Settings::Manager::getBool("window border", "Video");
    bool vsync = Settings::Manager::getBool("vsync", "Video");
    unsigned int antialiasing = std::max(0, Settings::Manager::getInt("antialiasing", "Video"));
    if (VR::getVR())
        // MSAA needs to happen in offscreen buffers.
        antialiasing = 0;

    int pos_x = SDL_WINDOWPOS_CENTERED_DISPLAY(screen),
        pos_y = SDL_WINDOWPOS_CENTERED_DISPLAY(screen);

    if(windowMode == Settings::WindowMode::Fullscreen || windowMode == Settings::WindowMode::WindowedFullscreen)
    {
        pos_x = SDL_WINDOWPOS_UNDEFINED_DISPLAY(screen);
        pos_y = SDL_WINDOWPOS_UNDEFINED_DISPLAY(screen);
    }

    Uint32 flags = SDL_WINDOW_OPENGL|SDL_WINDOW_SHOWN|SDL_WINDOW_RESIZABLE;
    if(windowMode == Settings::WindowMode::Fullscreen)
        flags |= SDL_WINDOW_FULLSCREEN;
    else if (windowMode == Settings::WindowMode::WindowedFullscreen)
        flags |= SDL_WINDOW_FULLSCREEN_DESKTOP;

    // Allows for Windows snapping features to properly work in borderless window
    SDL_SetHint("SDL_BORDERLESS_WINDOWED_STYLE", "1");
    SDL_SetHint("SDL_BORDERLESS_RESIZABLE_STYLE", "1");

    if (!windowBorder)
        flags |= SDL_WINDOW_BORDERLESS;

    SDL_SetHint(SDL_HINT_VIDEO_MINIMIZE_ON_FOCUS_LOSS,
                Settings::Manager::getBool("minimize on focus loss", "Video") ? "1" : "0");

    checkSDLError(SDL_GL_SetAttribute(SDL_GL_RED_SIZE, 8));
    checkSDLError(SDL_GL_SetAttribute(SDL_GL_GREEN_SIZE, 8));
    checkSDLError(SDL_GL_SetAttribute(SDL_GL_BLUE_SIZE, 8));
    checkSDLError(SDL_GL_SetAttribute(SDL_GL_ALPHA_SIZE, 0));
    checkSDLError(SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24));
    if (Debug::shouldDebugOpenGL())
        checkSDLError(SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, SDL_GL_CONTEXT_DEBUG_FLAG));

    if (antialiasing > 0)
    {
        checkSDLError(SDL_GL_SetAttribute(SDL_GL_MULTISAMPLEBUFFERS, 1));
        checkSDLError(SDL_GL_SetAttribute(SDL_GL_MULTISAMPLESAMPLES, antialiasing));
    }

    osg::ref_ptr<SDLUtil::GraphicsWindowSDL2> graphicsWindow;
    while (!graphicsWindow || !graphicsWindow->valid())
    {
        while (!mWindow)
        {
            mWindow = SDL_CreateWindow("OpenMW", pos_x, pos_y, width, height, flags);
            if (!mWindow)
            {
                // Try with a lower AA
                if (antialiasing > 0)
                {
                    Log(Debug::Warning) << "Warning: " << antialiasing << "x antialiasing not supported, trying " << antialiasing/2;
                    antialiasing /= 2;
                    Settings::Manager::setInt("antialiasing", "Video", antialiasing);
                    checkSDLError(SDL_GL_SetAttribute(SDL_GL_MULTISAMPLESAMPLES, antialiasing));
                    continue;
                }
                else
                {
                    std::stringstream error;
                    error << "Failed to create SDL window: " << SDL_GetError();
                    throw std::runtime_error(error.str());
                }
            }
        }

        setWindowIcon();

        osg::ref_ptr<osg::GraphicsContext::Traits> traits = new osg::GraphicsContext::Traits;
        SDL_GetWindowPosition(mWindow, &traits->x, &traits->y);
        SDL_GetWindowSize(mWindow, &traits->width, &traits->height);
        traits->windowName = SDL_GetWindowTitle(mWindow);
        traits->windowDecoration = !(SDL_GetWindowFlags(mWindow)&SDL_WINDOW_BORDERLESS);
        traits->screenNum = SDL_GetWindowDisplayIndex(mWindow);
        traits->vsync = vsync;
        traits->inheritedWindowData = new SDLUtil::GraphicsWindowSDL2::WindowData(mWindow);

        graphicsWindow = new SDLUtil::GraphicsWindowSDL2(traits);
        if (!graphicsWindow->valid()) throw std::runtime_error("Failed to create GraphicsContext");

        if (traits->samples < antialiasing)
        {
            Log(Debug::Warning) << "Warning: Framebuffer MSAA level is only " << traits->samples << "x instead of " << antialiasing << "x. Trying " << antialiasing / 2 << "x instead.";
            graphicsWindow->closeImplementation();
            SDL_DestroyWindow(mWindow);
            mWindow = nullptr;
            antialiasing /= 2;
            Settings::Manager::setInt("antialiasing", "Video", antialiasing);
            checkSDLError(SDL_GL_SetAttribute(SDL_GL_MULTISAMPLESAMPLES, antialiasing));
            continue;
        }

        if (traits->red < 8)
            Log(Debug::Warning) << "Warning: Framebuffer only has a " << traits->red << " bit red channel.";
        if (traits->green < 8)
            Log(Debug::Warning) << "Warning: Framebuffer only has a " << traits->green << " bit green channel.";
        if (traits->blue < 8)
            Log(Debug::Warning) << "Warning: Framebuffer only has a " << traits->blue << " bit blue channel.";
        if (traits->depth < 24)
            Log(Debug::Warning) << "Warning: Framebuffer only has " << traits->depth << " bits of depth precision.";

        traits->alpha = 0; // set to 0 to stop ScreenCaptureHandler reading the alpha channel
    }

    osg::ref_ptr<osg::Camera> camera = mViewer->getCamera();
    camera->setGraphicsContext(graphicsWindow);
    camera->setViewport(0, 0, graphicsWindow->getTraits()->width, graphicsWindow->getTraits()->height);

    osg::ref_ptr<SceneUtil::OperationSequence> realizeOperations = new SceneUtil::OperationSequence(false);
    mViewer->setRealizeOperation(realizeOperations);
    osg::ref_ptr<IdentifyOpenGLOperation> identifyOp = new IdentifyOpenGLOperation();
    realizeOperations->add(identifyOp);

    if (Debug::shouldDebugOpenGL())
        realizeOperations->add(new Debug::EnableGLDebugOperation());

    if (VR::getVR())
        realizeOperations->add(new InitializeVrOperation(this));
        
    realizeOperations->add(mSelectDepthFormatOperation);
    realizeOperations->add(mSelectColorFormatOperation);

    if (Stereo::getStereo())
    {
        realizeOperations->add(new InitializeStereoOperation());
        Stereo::setVertexBufferHint();
    }

    mViewer->realize();
    mGlMaxTextureImageUnits = identifyOp->getMaxTextureImageUnits();

    mViewer->getEventQueue()->getCurrentEventState()->setWindowRectangle(0, 0, graphicsWindow->getTraits()->width, graphicsWindow->getTraits()->height);
}

void OMW::Engine::setWindowIcon()
{
    std::ifstream windowIconStream;
    std::string windowIcon = (mResDir / "openmw.png").string();
    windowIconStream.open(windowIcon, std::ios_base::in | std::ios_base::binary);
    if (windowIconStream.fail())
        Log(Debug::Error) << "Error: Failed to open " << windowIcon;
    osgDB::ReaderWriter* reader = osgDB::Registry::instance()->getReaderWriterForExtension("png");
    if (!reader)
    {
        Log(Debug::Error) << "Error: Failed to read window icon, no png readerwriter found";
        return;
    }
    osgDB::ReaderWriter::ReadResult result = reader->readImage(windowIconStream);
    if (!result.success())
        Log(Debug::Error) << "Error: Failed to read " << windowIcon << ": " << result.message() << " code " << result.status();
    else
    {
        osg::ref_ptr<osg::Image> image = result.getImage();
        auto surface = SDLUtil::imageToSurface(image, true);
        SDL_SetWindowIcon(mWindow, surface.get());
    }
}

void OMW::Engine::prepareEngine()
{
    mStateManager = std::make_unique<MWState::StateManager>(mCfgMgr.getUserDataPath() / "saves", mContentFiles);
    mEnvironment.setStateManager(*mStateManager);

    mStereoManager = std::make_unique<Stereo::Manager>(mViewer);

    osg::ref_ptr<osg::Group> rootNode(new osg::Group);
    mViewer->setSceneData(rootNode);

    createWindow();

    mCallbackManager = std::make_unique<Misc::CallbackManager>(mViewer);

    mVFS = std::make_unique<VFS::Manager>(mFSStrict);

    VFS::registerArchives(mVFS.get(), mFileCollections, mArchives, true);

    mResourceSystem = std::make_unique<Resource::ResourceSystem>(mVFS.get());
    mResourceSystem->getSceneManager()->getShaderManager().setMaxTextureUnits(mGlMaxTextureImageUnits);
    mResourceSystem->getSceneManager()->setUnRefImageDataAfterApply(false); // keep to Off for now to allow better state sharing
    mResourceSystem->getSceneManager()->setFilterSettings(
        Settings::Manager::getString("texture mag filter", "General"),
        Settings::Manager::getString("texture min filter", "General"),
        Settings::Manager::getString("texture mipmap", "General"),
        Settings::Manager::getInt("anisotropy", "General")
    );
    mEnvironment.setResourceSystem(*mResourceSystem);

    int numThreads = Settings::Manager::getInt("preload num threads", "Cells");
    if (numThreads <= 0)
        throw std::runtime_error("Invalid setting: 'preload num threads' must be >0");
    mWorkQueue = new SceneUtil::WorkQueue(numThreads);
    mUnrefQueue = std::make_unique<SceneUtil::UnrefQueue>();

    mScreenCaptureOperation = new SceneUtil::AsyncScreenCaptureOperation(
        mWorkQueue,
        new SceneUtil::WriteScreenshotToFileOperation(
            mCfgMgr.getScreenshotPath().string(),
            Settings::Manager::getString("screenshot format", "General"),
            Settings::Manager::getBool("notify on saved screenshot", "General")
                    ? std::function<void (std::string)>(ScheduleNonDialogMessageBox {})
                    : std::function<void (std::string)>(IgnoreString {})
        )
    );

    mScreenCaptureHandler = new osgViewer::ScreenCaptureHandler(mScreenCaptureOperation);

    mViewer->addEventHandler(mScreenCaptureHandler);

    mLuaManager = std::make_unique<MWLua::LuaManager>(mVFS.get(), (mResDir / "lua_libs").string());
    mEnvironment.setLuaManager(*mLuaManager);

    // Create input and UI first to set up a bootstrapping environment for
    // showing a loading screen and keeping the window responsive while doing so

    std::string keybinderUser = (mCfgMgr.getUserConfigPath() / "input_v3.xml").string();
    bool keybinderUserExists = std::filesystem::exists(keybinderUser);
    if(!keybinderUserExists)
    {
        std::string input2 = (mCfgMgr.getUserConfigPath() / "input_v2.xml").string();
        if(std::filesystem::exists(input2)) {
            std::filesystem::copy_file(input2, keybinderUser);
            keybinderUserExists = std::filesystem::exists(keybinderUser);
            Log(Debug::Info) << "Loading keybindings file: " << keybinderUser;
        }
    }
    else
        Log(Debug::Info) << "Loading keybindings file: " << keybinderUser;

    const std::string userdefault = mCfgMgr.getUserConfigPath().string() + "/gamecontrollerdb.txt";
    const std::string localdefault = mCfgMgr.getLocalPath().string() + "/gamecontrollerdb.txt";
    const std::string globaldefault = mCfgMgr.getGlobalPath().string() + "/gamecontrollerdb.txt";

    std::string userGameControllerdb;
    if (std::filesystem::exists(userdefault))
        userGameControllerdb = userdefault;

    std::string gameControllerdb;
    if (std::filesystem::exists(localdefault))
        gameControllerdb = localdefault;
    else if (std::filesystem::exists(globaldefault))
        gameControllerdb = globaldefault;
    //else if it doesn't exist, pass in an empty string

    // gui needs our shaders path before everything else
    mResourceSystem->getSceneManager()->setShaderPath((mResDir / "shaders").string());

    osg::ref_ptr<osg::GLExtensions> exts = osg::GLExtensions::Get(0, false);
    bool shadersSupported = exts && (exts->glslLanguageVersion >= 1.2f);

#if OSG_VERSION_LESS_THAN(3, 6, 6)
    // hack fix for https://github.com/openscenegraph/OpenSceneGraph/issues/1028
    if (exts)
        exts->glRenderbufferStorageMultisampleCoverageNV = nullptr;
#endif

    osg::ref_ptr<osg::Group> guiRoot = new osg::Group;
    guiRoot->setName("GUI Root");
    guiRoot->setNodeMask(MWRender::Mask_GUI);
    mStereoManager->disableStereoForNode(guiRoot);
    rootNode->addChild(guiRoot);

    mWindowManager = std::make_unique<MWGui::WindowManager>(mWindow, mViewer, guiRoot, mResourceSystem.get(), mWorkQueue.get(),
                mCfgMgr.getLogPath().string() + std::string("/"),
                mScriptConsoleMode, mTranslationDataStorage, mEncoding,
                Version::getOpenmwVersionDescription(mResDir.string()), shadersSupported);
    mEnvironment.setWindowManager(*mWindowManager);

#ifdef USE_OPENXR
    const std::string xrinputuserdefault = mCfgMgr.getUserConfigPath().string() + "/xrcontrollersuggestions.xml";
    const std::string xrinputlocaldefault = mCfgMgr.getLocalPath().string() + "/xrcontrollersuggestions.xml";
    const std::string xrinputglobaldefault = mCfgMgr.getGlobalPath().string() + "/xrcontrollersuggestions.xml";

    std::string xrControllerSuggestions;
    if (boost::filesystem::exists(xrinputuserdefault))
        xrControllerSuggestions = xrinputuserdefault;
    else if (boost::filesystem::exists(xrinputlocaldefault))
        xrControllerSuggestions = xrinputlocaldefault;
    else if (boost::filesystem::exists(xrinputglobaldefault))
        xrControllerSuggestions = xrinputglobaldefault;
    else
        xrControllerSuggestions = ""; //if it doesn't exist, pass in an empty string

    Log(Debug::Verbose) << "xrinputuserdefault: " << xrinputuserdefault;
    Log(Debug::Verbose) << "xrinputlocaldefault: " << xrinputlocaldefault;
    Log(Debug::Verbose) << "xrinputglobaldefault: " << xrinputglobaldefault;

    mInputManager = std::make_unique<MWVR::VRInputManager>(mWindow, mViewer, mScreenCaptureHandler,
        mScreenCaptureOperation, keybinderUser, keybinderUserExists, userGameControllerdb, gameControllerdb, mGrab,
        xrControllerSuggestions);
#else
    mInputManager = std::make_unique<MWInput::InputManager>(mWindow, mViewer, mScreenCaptureHandler,
        mScreenCaptureOperation, keybinderUser, keybinderUserExists, userGameControllerdb, gameControllerdb, mGrab);
#endif
    mEnvironment.setInputManager(*mInputManager);

    // Create sound system
    mSoundManager = std::make_unique<MWSound::SoundManager>(mVFS.get(), mUseSound);
    mEnvironment.setSoundManager(*mSoundManager);

#ifdef USE_OPENXR
    mVrViewer = std::make_unique<VR::Viewer>(mXrSession, mViewer);
    mVrGUIManager = std::make_unique<MWVR::VRGUIManager>(mResourceSystem.get(), mVrViewer->getTrackersRoot());
    mVrViewer->configureCallbacks();
    auto cullMask = ~(MWRender::VisMask::Mask_UpdateVisitor | MWRender::VisMask::Mask_SimpleWater);
    cullMask &= ~MWRender::VisMask::Mask_GUI;
    cullMask |= MWRender::VisMask::Mask_3DGUI;
    mViewer->getCamera()->setCullMask(cullMask);
    mViewer->getCamera()->setCullMaskLeft(cullMask);
    mViewer->getCamera()->setCullMaskRight(cullMask);
#endif

    
//#ifdef USE_OPENXR
//    auto camera = std::make_unique<MWVR::VRCamera>(mViewer->getCamera());
//#else
    auto camera = std::make_unique<MWRender::Camera>(mViewer->getCamera());
//#endif
    

    if (!mSkipMenu)
    {
        const std::string& logo = Fallback::Map::getString("Movies_Company_Logo");
        if (!logo.empty())
            mWindowManager->playVideo(logo, true);
    }

    // Create the world
    [[maybe_unused]] auto* cameraTemp = camera.get();
    mWorld = std::make_unique<MWWorld::World>(mViewer, rootNode, std::move(camera), mResourceSystem.get(), mWorkQueue.get(), *mUnrefQueue,
        mFileCollections, mContentFiles, mGroundcoverFiles, mEncoder.get(), mActivationDistanceOverride, mCellName,
        mStartupScript, mResDir.string(), mCfgMgr.getUserDataPath().string());
    mWorld->setupPlayer();
    mWorld->setRandomSeed(mRandomSeed);
    mEnvironment.setWorld(*mWorld);

    mWindowManager->setStore(mWorld->getStore());
    mLuaManager->initL10n();
    mWindowManager->initUI();


    //Load translation data
    mTranslationDataStorage.setEncoder(mEncoder.get());
    for (size_t i = 0; i < mContentFiles.size(); i++)
      mTranslationDataStorage.loadTranslationData(mFileCollections, mContentFiles[i]);

    Compiler::registerExtensions (mExtensions);

    // Create script system
    mScriptContext = std::make_unique<MWScript::CompilerContext>(MWScript::CompilerContext::Type_Full);
    mScriptContext->setExtensions (&mExtensions);

    mScriptManager = std::make_unique<MWScript::ScriptManager>(mWorld->getStore(), *mScriptContext, mWarningsMode,
        mScriptBlacklistUse ? mScriptBlacklist : std::vector<std::string>());
    mEnvironment.setScriptManager(*mScriptManager);

    // Create game mechanics system
    mMechanicsManager = std::make_unique<MWMechanics::MechanicsManager>();
    mEnvironment.setMechanicsManager(*mMechanicsManager);

    // Create dialog system
    mJournal = std::make_unique<MWDialogue::Journal>();
    mEnvironment.setJournal(*mJournal);

    mDialogueManager = std::make_unique<MWDialogue::DialogueManager>(mExtensions, mTranslationDataStorage);
    mEnvironment.setDialogueManager(*mDialogueManager);

    // scripts
    if (mCompileAll)
    {
        std::pair<int, int> result = mScriptManager->compileAll();
        if (result.first)
            Log(Debug::Info)
                << "compiled " << result.second << " of " << result.first << " scripts ("
                << 100*static_cast<double> (result.second)/result.first
                << "%)";
    }
    if (mCompileAllDialogue)
    {
        std::pair<int, int> result = MWDialogue::ScriptTest::compileAll(&mExtensions, mWarningsMode);
        if (result.first)
            Log(Debug::Info)
                << "compiled " << result.second << " of " << result.first << " dialogue script/actor combinations a("
                << 100*static_cast<double> (result.second)/result.first
                << "%)";
    }

    mLuaManager->init();
    mLuaManager->loadPermanentStorage(mCfgMgr.getUserConfigPath().string());
}

class OMW::Engine::LuaWorker
{
public:
    explicit LuaWorker(Engine* engine) : mEngine(engine)
    {
        if (Settings::Manager::getInt("lua num threads", "Lua") > 0)
            mThread = std::thread([this]{ threadBody(); });
    };

    ~LuaWorker()
    {
        if (mThread && mThread->joinable())
        {
            Log(Debug::Error) << "Unexpected destruction of LuaWorker; likely there is an unhandled exception in the main thread.";
            join();
        }
    }

    void allowUpdate()
    {
        if (!mThread)
            return;
        {
            std::lock_guard<std::mutex> lk(mMutex);
            mUpdateRequest = true;
    }
        mCV.notify_one();
    }

    void finishUpdate()
    {
        if (mThread)
        {
            std::unique_lock<std::mutex> lk(mMutex);
            mCV.wait(lk, [&]{ return !mUpdateRequest; });
        }
        else
            update();
    };

    void join()
        {
        if (mThread)
        {
            {
                std::lock_guard<std::mutex> lk(mMutex);
                mJoinRequest = true;
            }
            mCV.notify_one();
            mThread->join();
    }
    }

private:
    void update()
    {
        const auto& viewer = mEngine->mViewer;
        const osg::Timer_t frameStart = viewer->getStartTick();
        const unsigned int frameNumber = viewer->getFrameStamp()->getFrameNumber();
        ScopedProfile<UserStatsType::Lua> profile(frameStart, frameNumber, *osg::Timer::instance(), *viewer->getViewerStats());

        mEngine->mLuaManager->update();
    }

    void threadBody()
        {
        while (true)
        {
            std::unique_lock<std::mutex> lk(mMutex);
            mCV.wait(lk, [&]{ return mUpdateRequest || mJoinRequest; });
            if (mJoinRequest)
                break;

            update();

            mUpdateRequest = false;
            lk.unlock();
            mCV.notify_one();
        }
    }

    Engine* mEngine;
    std::mutex mMutex;
    std::condition_variable mCV;
    bool mUpdateRequest = false;
    bool mJoinRequest = false;
    std::optional<std::thread> mThread;
};

// Initialise and enter main loop.
void OMW::Engine::go()
{
#ifdef USE_OPENXR
    VR::setVR(true);
#else
    VR::setVR(false);
#endif

    assert (!mContentFiles.empty());

    Log(Debug::Info) << "OSG version: " << osgGetVersion();
    SDL_version sdlVersion;
    SDL_GetVersion(&sdlVersion);
    Log(Debug::Info) << "SDL version: " << (int)sdlVersion.major << "." << (int)sdlVersion.minor << "." << (int)sdlVersion.patch;

    Misc::Rng::init(mRandomSeed);

    Settings::ShaderManager::get().load((mCfgMgr.getUserConfigPath() / "shaders.yaml").string());

    MWClass::registerClasses();

    // Create encoder
    mEncoder = std::make_unique<ToUTF8::Utf8Encoder>(mEncoding);

    // Setup viewer
    mViewer = new osgViewer::Viewer;
    mViewer->setReleaseContextAtEndOfFrameHint(false);

    // Do not try to outsmart the OS thread scheduler (see bug #4785).
    mViewer->setUseConfigureAffinity(false);

    mEnvironment.setFrameRateLimit(Settings::Manager::getFloat("framerate limit", "Video"));

    prepareEngine();

    std::ofstream stats;
    if (const auto path = std::getenv("OPENMW_OSG_STATS_FILE"))
    {
        stats.open(path, std::ios_base::out);
        if (stats.is_open())
            Log(Debug::Info) << "Stats will be written to: " << path;
        else
            Log(Debug::Warning) << "Failed to open file for stats: " << path;
    }

    // Setup profiler
    osg::ref_ptr<Resource::Profiler> statshandler = new Resource::Profiler(stats.is_open(), mVFS.get());

    initStatsHandler(*statshandler);

    mViewer->addEventHandler(statshandler);

    osg::ref_ptr<Resource::StatsHandler> resourceshandler = new Resource::StatsHandler(stats.is_open(), mVFS.get());
    mViewer->addEventHandler(resourceshandler);

    if (stats.is_open())
        Resource::CollectStatistics(mViewer);

    // TODO: Prevent Mask_GUI from being re-enabled instead
    if (VR::getVR())
    {
        mViewer->getCamera()->setCullMask(mViewer->getCamera()->getCullMask() & ~(MWRender::VisMask::Mask_GUI));
#ifdef USE_OPENXR
        static_cast<MWVR::VRInputManager*>(mInputManager.get())->calibrate();
#endif
    }

    // Start the game
    if (!mSaveGameFile.empty())
    {
        mStateManager->loadGame(mSaveGameFile);
    }
    else if (!mSkipMenu)
    {
        // start in main menu
        mWindowManager->pushGuiMode (MWGui::GM_MainMenu);
        mSoundManager->playTitleMusic();
        const std::string& logo = Fallback::Map::getString("Movies_Morrowind_Logo");
        if (!logo.empty())
            mWindowManager->playVideo(logo, /*allowSkipping*/true, /*overrideSounds*/false);
    }
    else
    {
        mStateManager->newGame (!mNewGame);
    }

    if (!mStartupScript.empty() && mStateManager->getState() == MWState::StateManager::State_Running)
    {
        mWindowManager->executeInConsole(mStartupScript);
    }

    LuaWorker luaWorker(this);  // starts a separate lua thread if "lua num threads" > 0

    // Start the main rendering loop
    double simulationTime = 0.0;
    Misc::FrameRateLimiter frameRateLimiter = Misc::makeFrameRateLimiter(mEnvironment.getFrameRateLimit());
    const std::chrono::steady_clock::duration maxSimulationInterval(std::chrono::milliseconds(200));
    while (!mViewer->done() && !mStateManager->hasQuitRequest())
    {
        const double dt = std::chrono::duration_cast<std::chrono::duration<double>>(std::min(
            frameRateLimiter.getLastFrameDuration(),
            maxSimulationInterval
        )).count() * mEnvironment.getWorld()->getSimulationTimeScale();

        mViewer->advance(simulationTime);

        if (!frame(dt))
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
            continue;
        }
        else
        {
            mViewer->eventTraversal();
            mViewer->updateTraversal();

            mWorld->updateWindowManager();

            luaWorker.allowUpdate();  // if there is a separate Lua thread, it starts the update now

            mViewer->renderingTraversals();

            luaWorker.finishUpdate();

            bool guiActive = mWindowManager->isGuiMode();
            if (!guiActive)
                simulationTime += dt;
        }

        if (stats)
        {
            const auto frameNumber = mViewer->getFrameStamp()->getFrameNumber();
            if (frameNumber >= 2)
            {
                mViewer->getViewerStats()->report(stats, frameNumber - 2);
                osgViewer::Viewer::Cameras cameras;
                mViewer->getCameras(cameras);
                for (auto camera : cameras)
                    camera->getStats()->report(stats, frameNumber - 2);
            }
        }

        frameRateLimiter.limit();
    }

    luaWorker.join();

    // Save user settings
    Settings::Manager::saveUser((mCfgMgr.getUserConfigPath() / "settings.cfg").string());
    Settings::ShaderManager::get().save();
    mLuaManager->savePermanentStorage(mCfgMgr.getUserConfigPath().string());

    Log(Debug::Info) << "Quitting peacefully.";
}

void OMW::Engine::setCompileAll (bool all)
{
    mCompileAll = all;
}

void OMW::Engine::setCompileAllDialogue (bool all)
{
    mCompileAllDialogue = all;
}

void OMW::Engine::setSoundUsage(bool soundUsage)
{
    mUseSound = soundUsage;
}

void OMW::Engine::setEncoding(const ToUTF8::FromType& encoding)
{
    mEncoding = encoding;
}

void OMW::Engine::setScriptConsoleMode (bool enabled)
{
    mScriptConsoleMode = enabled;
}

void OMW::Engine::setStartupScript (const std::string& path)
{
    mStartupScript = path;
}

void OMW::Engine::setActivationDistanceOverride (int distance)
{
    mActivationDistanceOverride = distance;
}

void OMW::Engine::setWarningsMode (int mode)
{
    mWarningsMode = mode;
}

void OMW::Engine::setScriptBlacklist (const std::vector<std::string>& list)
{
    mScriptBlacklist = list;
}

void OMW::Engine::setScriptBlacklistUse (bool use)
{
    mScriptBlacklistUse = use;
}

void OMW::Engine::setSaveGameFile(const std::string &savegame)
{
    mSaveGameFile = savegame;
}

void OMW::Engine::setRandomSeed(unsigned int seed)
{
    mRandomSeed = seed;
}

void OMW::Engine::configureVR(osg::GraphicsContext* gc)
{
#ifdef USE_OPENXR
    mVrTrackingManager = std::make_unique<VR::TrackingManager>();
    mXrInstance = std::make_unique<XR::Instance>(gc);
    mXrSession = mXrInstance->createSession();
    if (mXrSession->appShouldShareDepthInfo())
        mSelectDepthFormatOperation->setSupportedFormats(mXrInstance->platform().supportedDepthFormats());
    mSelectColorFormatOperation->setSupportedFormats({ GL_R11F_G11F_B10F });
#endif
}
