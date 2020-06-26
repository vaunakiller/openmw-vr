#include "openxrmanager.hpp"
#include "vrenvironment.hpp"
#include "openxrmanagerimpl.hpp"
#include "../mwinput/inputmanagerimp.hpp"

#include <components/debug/debuglog.hpp>

namespace MWVR
{
    OpenXRManager::OpenXRManager()
        : mPrivate(nullptr)
        , mMutex()
    {

    }

    OpenXRManager::~OpenXRManager()
    {

    }

    bool
        OpenXRManager::realized()
    {
        return !!mPrivate;
    }

    bool OpenXRManager::xrSessionRunning()
    {
        if (realized())
            return impl().xrSessionRunning();
        return false;
    }

    void OpenXRManager::handleEvents()
    {
        if (realized())
            return impl().handleEvents();
    }

    void OpenXRManager::waitFrame()
    {
        if (realized())
            return impl().waitFrame();
    }

    void OpenXRManager::beginFrame()
    {
        if (realized())
            return impl().beginFrame();
    }

    void OpenXRManager::endFrame(int64_t displayTime, int layerCount, const std::array<CompositionLayerProjectionView, 2>& layerStack)
    {
        if (realized())
            return impl().endFrame(displayTime, layerCount, layerStack);
    }

    void
        OpenXRManager::realize(
            osg::GraphicsContext* gc)
    {
        lock_guard lock(mMutex);
        if (!realized())
        {
            gc->makeCurrent();
            try {
                mPrivate = std::make_shared<OpenXRManagerImpl>();
            }
            catch (std::exception& e)
            {
                Log(Debug::Error) << "Exception thrown by OpenXR: " << e.what();
                osg::ref_ptr<osg::State> state = gc->getState();

            }
        }
    }

    void OpenXRManager::enablePredictions()
    {
        return impl().enablePredictions();
    }

    void OpenXRManager::disablePredictions()
    {
        return impl().disablePredictions();
    }

    std::array<View, 2> OpenXRManager::getPredictedViews(int64_t predictedDisplayTime, ReferenceSpace space)
    {
        return impl().getPredictedViews(predictedDisplayTime, space);
    }

    MWVR::Pose OpenXRManager::getPredictedHeadPose(int64_t predictedDisplayTime, ReferenceSpace space)
    {
        return impl().getPredictedHeadPose(predictedDisplayTime, space);
    }

    long long OpenXRManager::getLastPredictedDisplayTime()
    {
        return impl().getLastPredictedDisplayTime();
    }

    long long OpenXRManager::getLastPredictedDisplayPeriod()
    {
        return impl().getLastPredictedDisplayPeriod();
    }

    std::array<SwapchainConfig, 2> OpenXRManager::getRecommendedSwapchainConfig() const
    {
        return impl().getRecommendedSwapchainConfig();
    }

    void
        OpenXRManager::RealizeOperation::operator()(
            osg::GraphicsContext* gc)
    {
        auto* xr = Environment::get().getManager();
        xr->realize(gc);
    }

    bool
        OpenXRManager::RealizeOperation::realized()
    {
        auto* xr = Environment::get().getManager();
        return xr->realized();
    }

    void
        OpenXRManager::CleanupOperation::operator()(
            osg::GraphicsContext* gc)
    {

    }
}

