#include "openxrswapchainimpl.hpp"
#include "vrenvironment.hpp"

#include <components/debug/debuglog.hpp>

#include <Windows.h>

#include <openxr/openxr.h>
#include <openxr/openxr_platform.h>
#include <openxr/openxr_platform_defines.h>
#include <openxr/openxr_reflection.h>

namespace MWVR {


    OpenXRSwapchainImpl::OpenXRSwapchainImpl(osg::ref_ptr<osg::State> state, SwapchainConfig config)
        : mWidth((int)config.recommendedWidth)
        , mHeight((int)config.recommendedHeight)
        , mSamples((int)config.recommendedSamples)
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

        // List of supported color swapchain formats.
        constexpr int64_t SupportedColorSwapchainFormats[] = {
            GL_RGBA8,
            GL_RGBA8_SNORM,
        };

        auto swapchainFormatIt =
            std::find_first_of(swapchainFormats.begin(), swapchainFormats.end(), std::begin(SupportedColorSwapchainFormats),
                std::end(SupportedColorSwapchainFormats));
        if (swapchainFormatIt == swapchainFormats.end()) {
            Log(Debug::Error) << "No swapchain format supported at runtime";
        }

        mSwapchainColorFormat = *swapchainFormatIt;

        mSamples = Settings::Manager::getInt("antialiasing", "Video");

        while (mSamples > 0)
        {
            Log(Debug::Verbose) << "Creating swapchain with dimensions Width=" << mWidth << " Heigh=" << mHeight << " SampleCount=" << mSamples;
            // Create the swapchain.
            XrSwapchainCreateInfo swapchainCreateInfo{ XR_TYPE_SWAPCHAIN_CREATE_INFO };
            swapchainCreateInfo.arraySize = 1;
            swapchainCreateInfo.format = mSwapchainColorFormat;
            swapchainCreateInfo.width = mWidth;
            swapchainCreateInfo.height = mHeight;
            swapchainCreateInfo.mipCount = 1;
            swapchainCreateInfo.faceCount = 1;
            swapchainCreateInfo.sampleCount = 1;
            swapchainCreateInfo.usageFlags = XR_SWAPCHAIN_USAGE_SAMPLED_BIT | XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT;
            //CHECK_XRCMD(xrCreateSwapchain(xr->impl().xrSession(), &swapchainCreateInfo, &mSwapchain));
            auto res = xrCreateSwapchain(xr->impl().xrSession(), &swapchainCreateInfo, &mSwapchain);
            if (XR_SUCCEEDED(res))
                break;
            else
            {
                Log(Debug::Verbose) << "Failed to create swapchain with SampleCount=" << mSamples << ": " << XrResultString(res);
                mSamples /= 2;
                if(mSamples == 0)
                    std::runtime_error(XrResultString(res));
            }
        }

        uint32_t imageCount = 0;
        CHECK_XRCMD(xrEnumerateSwapchainImages(mSwapchain, 0, &imageCount, nullptr));

        mSwapchainImageBuffers.resize(imageCount, { XR_TYPE_SWAPCHAIN_IMAGE_OPENGL_KHR });
        CHECK_XRCMD(xrEnumerateSwapchainImages(mSwapchain, imageCount, &imageCount, reinterpret_cast<XrSwapchainImageBaseHeader*>(mSwapchainImageBuffers.data())));
        //for (const auto& swapchainImage : mSwapchainImageBuffers)
        //    mTextureBuffers.push_back(new VRTexture(state, swapchainImage.image, mWidth, mHeight, 0));
        for (unsigned i = 0; i < imageCount; i++)
            mRenderBuffers.emplace_back(new VRTexture(state, mWidth, mHeight, mSamples));

        mSubImage.swapchain = mSwapchain;
        mSubImage.imageRect.offset = { 0, 0 };
        mSubImage.imageRect.extent = { mWidth, mHeight };
    }

    OpenXRSwapchainImpl::~OpenXRSwapchainImpl()
    {
        if (mSwapchain)
            CHECK_XRCMD(xrDestroySwapchain(mSwapchain));
    }

    VRTexture* OpenXRSwapchainImpl::renderBuffer() const
    {
        return mRenderBuffers[mRenderBuffer].get();
    }

    uint32_t OpenXRSwapchainImpl::acquiredImage() const
    {
        if(isAcquired())
            return mSwapchainImageBuffers[mAcquiredImageIndex].image;
        throw std::logic_error("Swapbuffer not acquired before use");
    }

    bool OpenXRSwapchainImpl::isAcquired() const
    {
        return mIsAcquired;
    }

    void OpenXRSwapchainImpl::beginFrame(osg::GraphicsContext* gc)
    {
        mRenderBuffer = (mRenderBuffer + 1) % mRenderBuffers.size();
        renderBuffer()->beginFrame(gc);
    }

    int swapCount = 0;

    void OpenXRSwapchainImpl::endFrame(osg::GraphicsContext* gc)
    {
        // Blit frame to swapchain
        acquire(gc);
        renderBuffer()->endFrame(gc, acquiredImage());
        release(gc);
    }

    void OpenXRSwapchainImpl::acquire(osg::GraphicsContext* gc)
    {
        XrSwapchainImageAcquireInfo acquireInfo{ XR_TYPE_SWAPCHAIN_IMAGE_ACQUIRE_INFO };
        CHECK_XRCMD(xrAcquireSwapchainImage(mSwapchain, &acquireInfo, &mAcquiredImageIndex));

        XrSwapchainImageWaitInfo waitInfo{ XR_TYPE_SWAPCHAIN_IMAGE_WAIT_INFO };
        waitInfo.timeout = XR_INFINITE_DURATION;
        CHECK_XRCMD(xrWaitSwapchainImage(mSwapchain, &waitInfo));

        mIsAcquired = true;
    }

    void OpenXRSwapchainImpl::release(osg::GraphicsContext* gc)
    {
        mIsAcquired = false;

        XrSwapchainImageReleaseInfo releaseInfo{ XR_TYPE_SWAPCHAIN_IMAGE_RELEASE_INFO };
        CHECK_XRCMD(xrReleaseSwapchainImage(mSwapchain, &releaseInfo));
    }
}
