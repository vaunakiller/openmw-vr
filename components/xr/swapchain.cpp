#include "swapchain.hpp"
#include "debug.hpp"

#include <components/debug/debuglog.hpp>

namespace XR {
    Swapchain::Swapchain(XrSwapchain swapchain, std::vector<uint64_t> images, uint32_t width, uint32_t height, uint32_t samples, uint32_t format, uint32_t arraySize, uint32_t textureTarget)
        : VR::Swapchain(width, height, samples, format, arraySize, textureTarget, images.size(), false)
        , mXrSwapchain(swapchain)
        , mImages(images)
    {
    }

    Swapchain::~Swapchain()
    {
        xrDestroySwapchain(mXrSwapchain);
    }

    uint64_t Swapchain::beginFrame(osg::GraphicsContext* gc)
    {
        if (!mIsAcquired)
        {
            acquire();
        }
        if (mIsAcquired && !mIsReady)
        {
            wait();
        }

        return mImage = mImages[mAcquiredIndex];
    }

    void Swapchain::endFrame(osg::GraphicsContext* gc)
    {
        if (mIsReady)
        {
            release();
        }
        mImage = 0;
    }

    void Swapchain::acquire()
    {
        XrSwapchainImageAcquireInfo acquireInfo{};
        acquireInfo.type = XR_TYPE_SWAPCHAIN_IMAGE_ACQUIRE_INFO;
        mIsAcquired = XR_SUCCEEDED(CHECK_XRCMD(xrAcquireSwapchainImage(mXrSwapchain, &acquireInfo, &mAcquiredIndex)));
    }

    void Swapchain::wait()
    {
        XrSwapchainImageWaitInfo waitInfo{};
        waitInfo.type = XR_TYPE_SWAPCHAIN_IMAGE_WAIT_INFO;
        waitInfo.timeout = XR_INFINITE_DURATION;
        mIsReady = XR_SUCCEEDED(CHECK_XRCMD(xrWaitSwapchainImage(mXrSwapchain, &waitInfo)));
    }

    void Swapchain::release()
    {
        XrSwapchainImageReleaseInfo releaseInfo{};
        releaseInfo.type = XR_TYPE_SWAPCHAIN_IMAGE_RELEASE_INFO;

        mIsReady = !XR_SUCCEEDED(CHECK_XRCMD(xrReleaseSwapchainImage(mXrSwapchain, &releaseInfo)));
        if (!mIsReady)
        {
            mIsAcquired = false;
        }
    }

    bool Swapchain::isAcquired()
    {
        return mIsAcquired;
    }
}
