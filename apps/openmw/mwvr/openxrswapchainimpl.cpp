#include "openxrswapchainimpl.hpp"
#include "vrenvironment.hpp"
#include "vrframebuffer.hpp"

#include <components/debug/debuglog.hpp>

#include <Windows.h>
#include <objbase.h>

#include <openxr/openxr.h>
#include <openxr/openxr_platform.h>
#include <openxr/openxr_platform_defines.h>
#include <openxr/openxr_reflection.h>

namespace MWVR {
    OpenXRSwapchainImpl::OpenXRSwapchainImpl(osg::ref_ptr<osg::State> state, SwapchainConfig config)
        : mWidth(config.selectedWidth)
        , mHeight(config.selectedHeight)
        , mSamples(config.selectedSamples)
    {
        if (mWidth <= 0)
            throw std::invalid_argument("Width must be a positive integer");
        if (mHeight <= 0)
            throw std::invalid_argument("Height must be a positive integer");
        if (mSamples <= 0)
            throw std::invalid_argument("Samples must be a positive integer");

        auto* xr = Environment::get().getManager();

        // Select a swapchain format.
        uint32_t swapchainFormatCount;
        CHECK_XRCMD(xrEnumerateSwapchainFormats(xr->impl().xrSession(), 0, &swapchainFormatCount, nullptr));
        std::vector<int64_t> swapchainFormats(swapchainFormatCount);
        CHECK_XRCMD(xrEnumerateSwapchainFormats(xr->impl().xrSession(), (uint32_t)swapchainFormats.size(), &swapchainFormatCount, swapchainFormats.data()));

        // Find supported color swapchain format.
        constexpr int64_t RequestedColorSwapchainFormats[] = {
            GL_RGBA8,
            GL_RGBA8_SNORM,
            GL_SRGB8_ALPHA8,
            //GL_RGBA16,
            //0x881A // GL_RGBA16F
        };

        auto swapchainFormatIt =
            std::find_first_of(swapchainFormats.begin(), swapchainFormats.end(), std::begin(RequestedColorSwapchainFormats),
                std::end(RequestedColorSwapchainFormats));
        if (swapchainFormatIt == swapchainFormats.end()) {
            throw std::runtime_error("Swapchain color format not supported");
        }
        mSwapchainColorFormat = *swapchainFormatIt;
        Log(Debug::Verbose) << "Selected color format: " << std::dec << mSwapchainColorFormat << " (" << std::hex << mSwapchainColorFormat << ")" << std::dec;

        if (xr->xrExtensionIsEnabled(XR_KHR_COMPOSITION_LAYER_DEPTH_EXTENSION_NAME))
        {
            // Find supported depth swapchain format.
            constexpr int64_t RequestedDepthSwapchainFormats[] = {
                GL_DEPTH_COMPONENT24,
                GL_DEPTH_COMPONENT32F,
            };

            swapchainFormatIt =
                std::find_first_of(swapchainFormats.begin(), swapchainFormats.end(), std::begin(RequestedDepthSwapchainFormats),
                    std::end(RequestedDepthSwapchainFormats));
            if (swapchainFormatIt == swapchainFormats.end()) {
                mHaveDepthSwapchain = false;
                Log(Debug::Warning) << "OpenXR extension " << XR_KHR_COMPOSITION_LAYER_DEPTH_EXTENSION_NAME << " enabled, but no depth formats were found";
            }
            else
            {
                mSwapchainDepthFormat = *swapchainFormatIt;
                mHaveDepthSwapchain = true;
                Log(Debug::Verbose) << "Selected depth format: " << std::dec << mSwapchainDepthFormat << " (" << std::hex << mSwapchainDepthFormat << ")" << std::dec;
            }
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
            swapchainCreateInfo.format = mSwapchainColorFormat;
            swapchainCreateInfo.sampleCount = mSamples;
            swapchainCreateInfo.usageFlags = XR_SWAPCHAIN_USAGE_SAMPLED_BIT | XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT;
            auto res = xrCreateSwapchain(xr->impl().xrSession(), &swapchainCreateInfo, &mSwapchain);
            if (!XR_SUCCEEDED(res))
            {
                Log(Debug::Verbose) << "Failed to create swapchain with SampleCount=" << mSamples << ": " << XrResultString(res);
                mSamples /= 2;
                if (mSamples == 0)
                    throw std::runtime_error(XrResultString(res));
                continue;
            }
        }

        uint32_t imageCount = 0;
        CHECK_XRCMD(xrEnumerateSwapchainImages(mSwapchain, 0, &imageCount, nullptr));
        mSwapchainColorBuffers.resize(imageCount, { XR_TYPE_SWAPCHAIN_IMAGE_OPENGL_KHR });
        CHECK_XRCMD(xrEnumerateSwapchainImages(mSwapchain, imageCount, &imageCount, reinterpret_cast<XrSwapchainImageBaseHeader*>(mSwapchainColorBuffers.data())));
        mSubImage.swapchain = mSwapchain;
        mSubImage.imageRect.offset = { 0, 0 };
        mSubImage.imageRect.extent = { mWidth, mHeight };

        if (mHaveDepthSwapchain)
        {
            // Now create the swapchain of depth buffers if applicable
            if (mHaveDepthSwapchain)
            {
                swapchainCreateInfo.format = mSwapchainDepthFormat;
                swapchainCreateInfo.sampleCount = mSamples;
                swapchainCreateInfo.usageFlags = XR_SWAPCHAIN_USAGE_SAMPLED_BIT | XR_SWAPCHAIN_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
                auto res = xrCreateSwapchain(xr->impl().xrSession(), &swapchainCreateInfo, &mSwapchainDepth);
                if (!XR_SUCCEEDED(res))
                    throw std::runtime_error(XrResultString(res));
            }
            CHECK_XRCMD(xrEnumerateSwapchainImages(mSwapchainDepth, 0, &imageCount, nullptr));
            mSwapchainDepthBuffers.resize(imageCount, { XR_TYPE_SWAPCHAIN_IMAGE_OPENGL_KHR });
            CHECK_XRCMD(xrEnumerateSwapchainImages(mSwapchainDepth, imageCount, &imageCount, reinterpret_cast<XrSwapchainImageBaseHeader*>(mSwapchainDepthBuffers.data())));
            mSubImageDepth.swapchain = mSwapchainDepth;
            mSubImageDepth.imageRect.offset = { 0, 0 };
            mSubImageDepth.imageRect.extent = { mWidth, mHeight };
        }

        for (unsigned i = 0; i < imageCount; i++)
        {
            uint32_t colorBuffer = mSwapchainColorBuffers[i].image;
            uint32_t depthBuffer = mHaveDepthSwapchain ? mSwapchainDepthBuffers[i].image : 0;
            mRenderBuffers.emplace_back(new VRFramebuffer(state, mWidth, mHeight, mSamples, colorBuffer, depthBuffer));
        }
    }

    OpenXRSwapchainImpl::~OpenXRSwapchainImpl()
    {
        if (mSwapchain)
            CHECK_XRCMD(xrDestroySwapchain(mSwapchain));
    }

    VRFramebuffer* OpenXRSwapchainImpl::renderBuffer() const
    {
        checkAcquired();
        return mRenderBuffers[mAcquiredImageIndex].get();
    }

    uint32_t OpenXRSwapchainImpl::acquiredColorTexture() const
    {
        checkAcquired();
        return mSwapchainColorBuffers[mAcquiredImageIndex].image;
    }

    uint32_t OpenXRSwapchainImpl::acquiredDepthTexture() const
    {
        checkAcquired();
        return mSwapchainColorBuffers[mAcquiredImageIndex].image;
    }

    bool OpenXRSwapchainImpl::isAcquired() const
    {
        return mIsAcquired;
    }

    void OpenXRSwapchainImpl::beginFrame(osg::GraphicsContext* gc)
    {
        if (isAcquired())
            throw std::logic_error("Trying to acquire already acquired swapchain");
        acquire(gc);
        renderBuffer()->bindFramebuffer(gc, GL_FRAMEBUFFER_EXT);
    }

    int swapCount = 0;

    void OpenXRSwapchainImpl::endFrame(osg::GraphicsContext* gc)
    {
        checkAcquired();
        release(gc);
    }

    void OpenXRSwapchainImpl::acquire(osg::GraphicsContext*)
    {
        auto xr = Environment::get().getManager();
        XrSwapchainImageAcquireInfo acquireInfo{ XR_TYPE_SWAPCHAIN_IMAGE_ACQUIRE_INFO };
        XrSwapchainImageWaitInfo waitInfo{ XR_TYPE_SWAPCHAIN_IMAGE_WAIT_INFO };
        waitInfo.timeout = XR_INFINITE_DURATION;
        // I am trusting that the openxr runtime won't diverge these indices so long as these are always called together.
        // If some dumb ass implementation decides to violate this we'll just have to work around that if it actually happens.
        CHECK_XRCMD(xrAcquireSwapchainImage(mSwapchain, &acquireInfo, &mAcquiredImageIndex));
        CHECK_XRCMD(xrWaitSwapchainImage(mSwapchain, &waitInfo));
        xr->xrResourceAcquired();

        if (mHaveDepthSwapchain)
        {
            uint32_t depthIndex = 0;
            CHECK_XRCMD(xrAcquireSwapchainImage(mSwapchainDepth, &acquireInfo, &depthIndex));
            if (depthIndex != mAcquiredImageIndex)
                Log(Debug::Warning) << "Depth and color indices diverged";
            CHECK_XRCMD(xrWaitSwapchainImage(mSwapchainDepth, &waitInfo));
            xr->xrResourceAcquired();
        }

        mIsAcquired = true;
    }

    void OpenXRSwapchainImpl::release(osg::GraphicsContext*)
    {
        auto xr = Environment::get().getManager();
        mIsAcquired = false;

        XrSwapchainImageReleaseInfo releaseInfo{ XR_TYPE_SWAPCHAIN_IMAGE_RELEASE_INFO };
        CHECK_XRCMD(xrReleaseSwapchainImage(mSwapchain, &releaseInfo));
        xr->xrResourceReleased();
        if (mHaveDepthSwapchain)
        {
            CHECK_XRCMD(xrReleaseSwapchainImage(mSwapchainDepth, &releaseInfo));
            xr->xrResourceReleased();
        }
    }
    void OpenXRSwapchainImpl::checkAcquired() const
    {
        if (!isAcquired())
            throw std::logic_error("Swapchain must be acquired before use. Call between OpenXRSwapchain::beginFrame() and OpenXRSwapchain::endFrame()");
    }
}
