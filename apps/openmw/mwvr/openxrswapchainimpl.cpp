#include "openxrswapchainimpl.hpp"
#include "openxrdebug.hpp"
#include "vrenvironment.hpp"
#include "vrframebuffer.hpp"

#include <components/debug/debuglog.hpp>

namespace MWVR {
    OpenXRSwapchainImpl::OpenXRSwapchainImpl(osg::ref_ptr<osg::State> state, SwapchainConfig config)
        : mConfig(config)
    {
        if (mConfig.selectedWidth <= 0)
            throw std::invalid_argument("Width must be a positive integer");
        if (mConfig.selectedHeight <= 0)
            throw std::invalid_argument("Height must be a positive integer");
        if (mConfig.selectedSamples <= 0)
            throw std::invalid_argument("Samples must be a positive integer");

        mSwapchain.reset(new SwapchainPrivate(state, mConfig, SwapchainPrivate::Use::COLOR));
        mConfig.selectedSamples = mSwapchain->samples();

        auto* xr = Environment::get().getManager();

        if (xr->xrExtensionIsEnabled(XR_KHR_COMPOSITION_LAYER_DEPTH_EXTENSION_NAME))
        {
            try
            {
                mSwapchainDepth.reset(new SwapchainPrivate(state, mConfig, SwapchainPrivate::Use::DEPTH));
            }
            catch (std::exception& e)
            {
                Log(Debug::Warning) << "XR_KHR_composition_layer_depth was enabled but creating depth swapchain failed: " << e.what();
                mSwapchainDepth = nullptr;
            }
        }

        mFramebuffer.reset(new VRFramebuffer(state, mSwapchain->width(), mSwapchain->height(), mSwapchain->samples()));
        mFramebuffer->createColorBuffer(state->getGraphicsContext());
        mFramebuffer->createDepthBuffer(state->getGraphicsContext());
    }

    OpenXRSwapchainImpl::~OpenXRSwapchainImpl()
    {
    }

    VRFramebuffer* OpenXRSwapchainImpl::framebuffer() const
    {
        checkAcquired();
        // Note that I am trusting that the openxr runtime won't diverge the indexes of the depth and color swapchains so long as these are always acquired together.
        // If some dumb ass implementation decides to violate this we'll just have to work around that if it actually happens.
        return mFramebuffer.get();
    }

    bool OpenXRSwapchainImpl::isAcquired() const
    {
        return mFormallyAcquired;
    }

    void OpenXRSwapchainImpl::beginFrame(osg::GraphicsContext* gc)
    {
        acquire(gc);

        mFramebuffer->bindFramebuffer(gc, GL_FRAMEBUFFER_EXT);
    }

    int swapCount = 0;

    void OpenXRSwapchainImpl::endFrame(osg::GraphicsContext* gc)
    {
        checkAcquired();
        release(gc);
    }

    void OpenXRSwapchainImpl::acquire(osg::GraphicsContext* gc)
    {
        if (isAcquired())
            throw std::logic_error("Trying to acquire already acquired swapchain");

        // The openxr runtime may fail to acquire/release.
        // Do not re-acquire a swapchain before having successfully released it.
        // Lest the swapchain fall out of sync.
        if (!mShouldRelease)
        {
            mSwapchain->acquire(gc);
            mShouldRelease = mSwapchain->isAcquired();
            if (mSwapchainDepth && mSwapchain->isAcquired())
            {
                mSwapchainDepth->acquire(gc);
                mShouldRelease = mSwapchainDepth->isAcquired();
            }
        }

        mFormallyAcquired = true;
    }

    void OpenXRSwapchainImpl::release(osg::GraphicsContext* gc)
    {
        // The openxr runtime may fail to acquire/release.
        // Do not release a swapchain before having successfully acquire it.
        if (mShouldRelease)
        {
            mSwapchain->blitAndRelease(gc, *mFramebuffer);
            mShouldRelease = mSwapchain->isAcquired();
            if (mSwapchainDepth)
            {
                mSwapchainDepth->blitAndRelease(gc, *mFramebuffer);
                mShouldRelease = mSwapchainDepth->isAcquired();
            }
        }

        mFormallyAcquired = false;
    }

    void OpenXRSwapchainImpl::checkAcquired() const
    {
        if (!isAcquired())
            throw std::logic_error("Swapchain must be acquired before use. Call between OpenXRSwapchain::beginFrame() and OpenXRSwapchain::endFrame()");
    }

    OpenXRSwapchainImpl::SwapchainPrivate::SwapchainPrivate(osg::ref_ptr<osg::State> state, SwapchainConfig config, Use use)
        : mImages()
        , mWidth(config.selectedWidth)
        , mHeight(config.selectedHeight)
        , mSamples(config.selectedSamples)
        , mUsage(use)
    {
        auto* xr = Environment::get().getManager();

        // Select a swapchain format.
        if (use == Use::COLOR)
            mFormat = xr->selectColorFormat();
        else
            mFormat = xr->selectDepthFormat();
        std::string typeString = use == Use::COLOR ? "color" : "depth";

        if (mFormat == 0) {
            throw std::runtime_error(std::string("Swapchain ") + typeString + " format not supported");
        }
        Log(Debug::Verbose) << "Selected " << typeString << " format: " << std::dec << mFormat << " (" << std::hex << mFormat << ")" << std::dec;

        if (xr->xrExtensionIsEnabled(XR_KHR_COMPOSITION_LAYER_DEPTH_EXTENSION_NAME))
        {
            // TODO
        }

        XrSwapchainCreateInfo swapchainCreateInfo{ XR_TYPE_SWAPCHAIN_CREATE_INFO };
        swapchainCreateInfo.arraySize = 1;
        swapchainCreateInfo.width = mWidth;
        swapchainCreateInfo.height = mHeight;
        swapchainCreateInfo.mipCount = 1;
        swapchainCreateInfo.faceCount = 1;

        while (mSamples > 0 && mSwapchain == XR_NULL_HANDLE)
        {
            Log(Debug::Verbose) << "Creating swapchain with dimensions Width=" << mWidth << " Heigh=" << mHeight << " SampleCount=" << mSamples;
            // First create the swapchain of color buffers.
            swapchainCreateInfo.format = mFormat;
            swapchainCreateInfo.sampleCount = mSamples;
            if(use == Use::COLOR)
                swapchainCreateInfo.usageFlags = XR_SWAPCHAIN_USAGE_SAMPLED_BIT | XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT;
            else
                swapchainCreateInfo.usageFlags = XR_SWAPCHAIN_USAGE_SAMPLED_BIT | XR_SWAPCHAIN_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
            auto res = xrCreateSwapchain(xr->impl().xrSession(), &swapchainCreateInfo, &mSwapchain);
            if (!XR_SUCCEEDED(res))
            {
                Log(Debug::Verbose) << "Failed to create swapchain with SampleCount=" << mSamples << ": " << XrResultString(res);
                mSamples /= 2;
                if (mSamples == 0)
                    throw std::runtime_error(XrResultString(res));
                continue;
            }
            VrDebug::setName(mSwapchain, "OpenMW XR Color Swapchain " + config.name);
        }

        // TODO: here
        mImages = OpenXRSwapchainImage::enumerateSwapchainImages(state->getGraphicsContext(), mSwapchain, swapchainCreateInfo);
        mSubImage.swapchain = mSwapchain;
        mSubImage.imageRect.offset = { 0, 0 };
        mSubImage.imageRect.extent = { mWidth, mHeight };
    }

    OpenXRSwapchainImpl::SwapchainPrivate::~SwapchainPrivate()
    {
        if (mSwapchain)
            CHECK_XRCMD(xrDestroySwapchain(mSwapchain));
    }

    uint32_t OpenXRSwapchainImpl::SwapchainPrivate::count() const
    {
        return mImages.size();
    }

    bool OpenXRSwapchainImpl::SwapchainPrivate::isAcquired() const
    {
        return mIsReady;
    }

    void OpenXRSwapchainImpl::SwapchainPrivate::acquire(osg::GraphicsContext* gc)
    {
        auto xr = Environment::get().getManager();
        XrSwapchainImageAcquireInfo acquireInfo{ XR_TYPE_SWAPCHAIN_IMAGE_ACQUIRE_INFO };
        XrSwapchainImageWaitInfo waitInfo{ XR_TYPE_SWAPCHAIN_IMAGE_WAIT_INFO };
        waitInfo.timeout = XR_INFINITE_DURATION;
        if (!mIsIndexAcquired)
        {
            mIsIndexAcquired = XR_SUCCEEDED(CHECK_XRCMD(xrAcquireSwapchainImage(mSwapchain, &acquireInfo, &mAcquiredIndex)));
            if (mIsIndexAcquired)
                xr->xrResourceAcquired();
        }
        if (mIsIndexAcquired && !mIsReady)
        {
            mIsReady = XR_SUCCEEDED(CHECK_XRCMD(xrWaitSwapchainImage(mSwapchain, &waitInfo)));
        }
    }
    void OpenXRSwapchainImpl::SwapchainPrivate::blitAndRelease(osg::GraphicsContext* gc, VRFramebuffer& readBuffer)
    {
        auto xr = Environment::get().getManager();

        XrSwapchainImageReleaseInfo releaseInfo{ XR_TYPE_SWAPCHAIN_IMAGE_RELEASE_INFO };
        if (mIsReady)
        {
            mImages[mAcquiredIndex]->blit(gc, readBuffer);

            mIsReady = !XR_SUCCEEDED(CHECK_XRCMD(xrReleaseSwapchainImage(mSwapchain, &releaseInfo)));
            if (!mIsReady)
            {
                mIsIndexAcquired = false;
                xr->xrResourceReleased();
            }
        }
    }
    void OpenXRSwapchainImpl::SwapchainPrivate::checkAcquired() const
    {
        if (!isAcquired())
            throw std::logic_error("Swapchain must be acquired before use. Call between OpenXRSwapchain::beginFrame() and OpenXRSwapchain::endFrame()");
    }
}
