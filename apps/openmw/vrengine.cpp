#include "engine.hpp"

#include "mwvr/vrviewer.hpp"

#ifndef USE_OPENXR
#error "USE_OPENXR not defined"
#endif

void OMW::Engine::initVr()
{
    if (!mViewer)
        throw std::logic_error("mViewer must be initialized before calling initVr()");

    mXrEnvironment.setViewer(new MWVR::VRViewer(mViewer));
}
