#include "engine.hpp"
#include "mwvr/openxrmanager.hpp"
#include "mwvr/vrsession.hpp"
#include "mwvr/vrviewer.hpp"
#include "mwvr/vrgui.hpp"

#ifndef USE_OPENXR
#error "USE_OPENXR not defined"
#endif

void OMW::Engine::initVr()
{
    if (!mViewer)
        throw std::logic_error("mViewer must be initialized before calling initVr()");

    mXrEnvironment.setManager(new MWVR::OpenXRManager);

    // Ref: https://wiki.openmw.org/index.php?title=Measurement_Units
    float unitsPerYard = 64.f;
    float yardsPerMeter = 0.9144f;
    float unitsPerMeter = unitsPerYard / yardsPerMeter;
    mXrEnvironment.setUnitsPerMeter(unitsPerMeter);
    mXrEnvironment.setSession(new MWVR::VRSession());
    mXrEnvironment.setViewer(new MWVR::VRViewer(mViewer));
}
