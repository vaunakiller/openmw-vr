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
    osg::ref_ptr<MWVR::OpenXRManager::RealizeOperation> realizeOperation = new MWVR::OpenXRManager::RealizeOperation(mXR);
    mViewer->setRealizeOperation(realizeOperation);
    mXRViewer = new MWVR::OpenXRViewer(mXR, realizeOperation, mViewer, 1.f);

    // Viewers must be the top node of the scene.
    //mViewer->setSceneData(mXRViewer);
}
