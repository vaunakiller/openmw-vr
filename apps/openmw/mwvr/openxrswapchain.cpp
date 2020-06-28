#include "openxrswapchain.hpp"
#include "openxrswapchainimpl.hpp"
#include "openxrmanager.hpp"
#include "openxrmanagerimpl.hpp"
#include "vrenvironment.hpp"

#include <components/debug/debuglog.hpp>

#include <Windows.h>

#include <openxr/openxr.h>
#include <openxr/openxr_platform.h>
#include <openxr/openxr_platform_defines.h>
#include <openxr/openxr_reflection.h>

namespace MWVR {
    OpenXRSwapchain::OpenXRSwapchain(osg::ref_ptr<osg::State> state, SwapchainConfig config)
        : mPrivate(new OpenXRSwapchainImpl(state, config))
    {
    }

    OpenXRSwapchain::~OpenXRSwapchain()
    {
    }

    void OpenXRSwapchain::beginFrame(osg::GraphicsContext* gc)
    {
        return impl().beginFrame(gc);
    }

    void OpenXRSwapchain::endFrame(osg::GraphicsContext* gc)
    {
        return impl().endFrame(gc);
    }

    uint32_t OpenXRSwapchain::acquiredColorTexture() const
    {
        return impl().acquiredColorTexture();
    }

    uint32_t OpenXRSwapchain::acquiredDepthTexture() const
    {
        return impl().acquiredDepthTexture();
    }

    int OpenXRSwapchain::width() const
    {
        return impl().width();
    }

    int OpenXRSwapchain::height() const
    {
        return impl().height();
    }

    int OpenXRSwapchain::samples() const
    {
        return impl().samples();
    }

    bool OpenXRSwapchain::isAcquired() const
    {
        return impl().isAcquired();
    }

    VRFramebuffer* OpenXRSwapchain::renderBuffer() const
    {
        return impl().renderBuffer();
    }
}
