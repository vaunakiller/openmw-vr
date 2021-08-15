#include "openxrswapchain.hpp"
#include "openxrmanager.hpp"
#include "openxrmanagerimpl.hpp"
#include "vrenvironment.hpp"

#include <components/debug/debuglog.hpp>

namespace MWVR {
    OpenXRSwapchain::OpenXRSwapchain(XrSwapchain swapchain, std::vector<uint64_t> images, uint32_t width, uint32_t height, uint32_t samples, uint32_t format)
        : Swapchain(width, height, samples, format, false)
        , mXrSwapchain(swapchain)
        , mImages(images)
    {
    }

    OpenXRSwapchain::~OpenXRSwapchain()
    {
        xrDestroySwapchain(mXrSwapchain);
    }

    uint64_t OpenXRSwapchain::beginFrame(osg::GraphicsContext* gc)
    {
        if (!mIsAcquired)
        {
            acquire();
        }
        if (mIsAcquired && !mIsReady)
        {
            wait();
        }

        return mImages[mAcquiredIndex];
    }

    void OpenXRSwapchain::endFrame(osg::GraphicsContext* gc)
    {
        if (mIsReady)
        {
            release();
        }
    }

    void OpenXRSwapchain::acquire()
    {
        XrSwapchainImageAcquireInfo acquireInfo{ XR_TYPE_SWAPCHAIN_IMAGE_ACQUIRE_INFO };
        mIsAcquired = XR_SUCCEEDED(CHECK_XRCMD(xrAcquireSwapchainImage(mXrSwapchain, &acquireInfo, &mAcquiredIndex)));
        //if (mIsIndexAcquired)
        // TODO:   xr->xrResourceAcquired();
    }

    void OpenXRSwapchain::wait()
    {
        XrSwapchainImageWaitInfo waitInfo{ XR_TYPE_SWAPCHAIN_IMAGE_WAIT_INFO };
        waitInfo.timeout = XR_INFINITE_DURATION;
        mIsReady = XR_SUCCEEDED(CHECK_XRCMD(xrWaitSwapchainImage(mXrSwapchain, &waitInfo)));
    }

    void OpenXRSwapchain::release()
    {
        XrSwapchainImageReleaseInfo releaseInfo{ XR_TYPE_SWAPCHAIN_IMAGE_RELEASE_INFO };
        //mImages[mAcquiredIndex]->blit(gc, readBuffer, mConfig.offsetWidth, mConfig.offsetHeight);

        mIsReady = !XR_SUCCEEDED(CHECK_XRCMD(xrReleaseSwapchainImage(mXrSwapchain, &releaseInfo)));
        if (!mIsReady)
        {
            mIsAcquired = false;
        // TODO:    xr->xrResourceReleased();
        }
    }

    bool OpenXRSwapchain::isAcquired()
    {
        return mIsAcquired;
    }
}
