#include "engine.hpp"
#include "mwbase/environment.hpp"
#include "mwvr/openxrmanager.hpp"
#include "mwvr/openxrsession.hpp"

#ifndef USE_OPENXR
#error "USE_OPENXR not defined"
#endif

void OMW::Engine::initVr()
{
    if (!mViewer)
        throw std::logic_error("mViewer must be initialized before calling initVr()");

    mXR = new MWVR::OpenXRManager();
    mEnvironment.setXRSession(new MWVR::OpenXRSession(mXR));

    // Ref: https://wiki.openmw.org/index.php?title=Measurement_Units
    float unitsPerYard = 64.f;
    float yardsPerMeter = 0.9144f;
    float unitsPerMeter = unitsPerYard / yardsPerMeter;
    mXRViewer = new MWVR::OpenXRViewer(mXR, mViewer, unitsPerMeter);

}
