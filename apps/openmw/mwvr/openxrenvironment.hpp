#ifndef OPENXR_ENVIRONMENT_H
#define OPENXR_ENVIRONMENT_H

namespace MWVR
{
    class OpenXRInputManager;
    class OpenXRSession;
    class OpenXRMenuManager;
    class OpenXRViewer;
    class OpenXRManager;

    /// \brief Central hub for mw openxr subsystems
    ///
    /// This class allows each mw openxr subsystem to access any others subsystem's top-level manager class.
    ///
    /// \attention OpenXREnvironment takes ownership of the manager class instances it is handed over in
    /// the set* functions.
    class OpenXREnvironment
    {

        static OpenXREnvironment*sThis;

        OpenXREnvironment(const OpenXREnvironment&) = delete;
        ///< not implemented

        OpenXREnvironment& operator= (const OpenXREnvironment&) = delete;
        ///< not implemented

    public:

        OpenXREnvironment();

        ~OpenXREnvironment();

        void cleanup();
        ///< Delete all mwvr-subsystems.

        static const OpenXREnvironment& get();
        ///< Return instance of this class.

        MWVR::OpenXRInputManager* getInputManager() const;

        // The OpenXRInputManager supplants the regular input manager
        // which is stored in MWBase::Environment
        // void setInputManager(MWVR::OpenXRInputManager*);

        MWVR::OpenXRMenuManager* getMenuManager() const;
        void setMenuManager(MWVR::OpenXRMenuManager* xrMenuManager);

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
        MWVR::OpenXRMenuManager* mMenuManager{ nullptr };
        MWVR::OpenXRViewer* mViewer{ nullptr };
        MWVR::OpenXRManager* mOpenXRManager{ nullptr };
        float mUnitsPerMeter{ 1.f };
    };
}

#endif
