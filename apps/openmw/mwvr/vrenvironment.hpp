#ifndef MWVR_ENVIRONMENT_H
#define MWVR_ENVIRONMENT_H

namespace MWVR
{
    class VRAnimation;
    class OpenXRInputManager;
    class OpenXRSession;
    class VRGUIManager;
    class OpenXRViewer;
    class OpenXRManager;

    /// \brief Central hub for mw openxr subsystems
    ///
    /// This class allows each mw openxr subsystem to access any others subsystem's top-level manager class.
    ///
    /// \attention Environment takes ownership of the manager class instances it is handed over in
    /// the set* functions.
    class Environment
    {

        static Environment* sThis;

        Environment(const Environment&) = delete;
        ///< not implemented

        Environment& operator= (const Environment&) = delete;
        ///< not implemented

    public:

        Environment();

        ~Environment();

        void cleanup();
        ///< Delete all mwvr-subsystems.

        static Environment& get();
        ///< Return instance of this class.

        MWVR::OpenXRInputManager* getInputManager() const;

        // The OpenXRInputManager supplants the regular input manager
        // which is stored in MWBase::Environment
        // void setInputManager(MWVR::OpenXRInputManager*);

        MWVR::VRGUIManager* getGUIManager() const;
        void setGUIManager(MWVR::VRGUIManager* xrGUIManager);

        MWVR::VRAnimation* getPlayerAnimation() const;
        void setPlayerAnimation(MWVR::VRAnimation* xrAnimation);

        MWVR::OpenXRSession* getSession() const;
        void setSession(MWVR::OpenXRSession* xrSession);

        MWVR::OpenXRViewer* getViewer() const;
        void setViewer(MWVR::OpenXRViewer* xrViewer);

        MWVR::OpenXRManager* getManager() const;
        void setManager(MWVR::OpenXRManager* xrManager);

        float unitsPerMeter() const;
        void setUnitsPerMeter(float unitsPerMeter);

    private:
        MWVR::OpenXRSession* mSession{ nullptr };
        MWVR::VRGUIManager* mGUIManager{ nullptr };
        MWVR::VRAnimation* mPlayerAnimation{ nullptr };
        MWVR::OpenXRViewer* mViewer{ nullptr };
        MWVR::OpenXRManager* mOpenXRManager{ nullptr };
        float mUnitsPerMeter{ 1.f };
    };
}

#endif
