#include "engine.hpp"
#include "mwvr/openxrmanager.hpp"
#include "mwvr/openxrsession.hpp"
#include "mwvr/openxrviewer.hpp"

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
    mXrEnvironment.setSession(new MWVR::OpenXRSession());
    mXrEnvironment.setViewer(new MWVR::OpenXRViewer(mViewer));

}
