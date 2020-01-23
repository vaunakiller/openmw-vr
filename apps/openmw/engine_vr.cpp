#include "engine.hpp"
#include "mwvr/openxrmanager.hpp"

#ifndef USE_OPENXR
#error "USE_OPENXR not defined"
#endif

void OMW::Engine::initVr()
{
    if (!mViewer)
        throw std::logic_error("mViewer must be initialized before calling initVr()");

    mXR = new MWVR::OpenXRManager();
    mXRViewer = new MWVR::OpenXRViewer(mXR, mViewer, 1.f);

}
