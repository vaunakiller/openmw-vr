#include "openxrswapchain.hpp"
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

    class OpenXRSwapchainImpl
    {
    public:
        enum SubView
        {
            LEFT_VIEW = 0,
            RIGHT_VIEW = 1,
            SUBVIEW_MAX = RIGHT_VIEW, //!< Used to size subview arrays. Not a valid input.
        };

        OpenXRSwapchainImpl(osg::ref_ptr<osg::State> state, OpenXRSwapchain::Config config);
        ~OpenXRSwapchainImpl();

        void beginFrame(osg::GraphicsContext* gc);
        void endFrame(osg::GraphicsContext* gc);
        void acquire(osg::GraphicsContext* gc);
        void release(osg::GraphicsContext* gc);

        VRTexture* renderBuffer() const;

        uint32_t acquiredImage() const;

        bool isAcquired() const;

        XrSwapchain mSwapchain = XR_NULL_HANDLE;
        std::vector<XrSwapchainImageOpenGLKHR> mSwapchainImageBuffers{};
        //std::vector<osg::ref_ptr<VRTexture> > mTextureBuffers{};
        XrSwapchainSubImage mSubImage{};
        int32_t mWidth = -1;
        int32_t mHeight = -1;
        int32_t mSamples = -1;
        int64_t mSwapchainColorFormat = -1;
        uint32_t mFBO = 0;
        std::vector<std::unique_ptr<VRTexture> > mRenderBuffers{};
        int mRenderBuffer{ 0 };
        uint32_t mAcquiredImageIndex{ 0 };
        bool mIsAcquired{ false };
    };

    OpenXRSwapchainImpl::OpenXRSwapchainImpl(osg::ref_ptr<osg::State> state, OpenXRSwapchain::Config config)
        : mWidth(config.width)
        , mHeight(config.height)
        , mSamples(config.samples)
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
        CHECK_XRCMD(xrEnumerateSwapchainFormats(xr->impl().mSession, 0, &swapchainFormatCount, nullptr));
        std::vector<int64_t> swapchainFormats(swapchainFormatCount);
        CHECK_XRCMD(xrEnumerateSwapchainFormats(xr->impl().mSession, (uint32_t)swapchainFormats.size(), &swapchainFormatCount, swapchainFormats.data()));

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

        Log(Debug::Verbose) << "Creating swapchain with dimensions Width=" << mWidth << " Heigh=" << mHeight << " SampleCount=" << mSamples;

        // Create the swapchain.
        XrSwapchainCreateInfo swapchainCreateInfo{ XR_TYPE_SWAPCHAIN_CREATE_INFO };
        swapchainCreateInfo.arraySize = 1;
        swapchainCreateInfo.format = mSwapchainColorFormat;
        swapchainCreateInfo.width = mWidth;
        swapchainCreateInfo.height = mHeight;
        swapchainCreateInfo.mipCount = 1;
        swapchainCreateInfo.faceCount = 1;
        swapchainCreateInfo.sampleCount = mSamples;
        swapchainCreateInfo.usageFlags = XR_SWAPCHAIN_USAGE_SAMPLED_BIT | XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT;
        CHECK_XRCMD(xrCreateSwapchain(xr->impl().mSession, &swapchainCreateInfo, &mSwapchain));

        uint32_t imageCount = 0;
        CHECK_XRCMD(xrEnumerateSwapchainImages(mSwapchain, 0, &imageCount, nullptr));

        mSwapchainImageBuffers.resize(imageCount, { XR_TYPE_SWAPCHAIN_IMAGE_OPENGL_KHR });
        CHECK_XRCMD(xrEnumerateSwapchainImages(mSwapchain, imageCount, &imageCount, reinterpret_cast<XrSwapchainImageBaseHeader*>(mSwapchainImageBuffers.data())));
        //for (const auto& swapchainImage : mSwapchainImageBuffers)
        //    mTextureBuffers.push_back(new VRTexture(state, swapchainImage.image, mWidth, mHeight, 0));
        for (unsigned i = 0; i < imageCount; i++)
            mRenderBuffers.emplace_back(new VRTexture(state, mWidth, mHeight, 0));

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

    OpenXRSwapchain::OpenXRSwapchain(osg::ref_ptr<osg::State> state, OpenXRSwapchain::Config config)
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

    void OpenXRSwapchain::acquire(osg::GraphicsContext* gc)
    {
        return impl().acquire(gc);
    }

    void OpenXRSwapchain::release(osg::GraphicsContext* gc)
    {
        return impl().release(gc);
    }

    const XrSwapchainSubImage& 
        OpenXRSwapchain::subImage(void) const
    {
        return impl().mSubImage;
    }

    uint32_t OpenXRSwapchain::acquiredImage() const
    {
        return impl().acquiredImage();
    }

    int OpenXRSwapchain::width() const
    {
        return impl().mWidth;
    }

    int OpenXRSwapchain::height() const
    {
        return impl().mHeight;
    }

    int OpenXRSwapchain::samples() const
    {
        return impl().mSamples;
    }

    bool OpenXRSwapchain::isAcquired() const
    {
        return impl().isAcquired();
    }

    VRTexture* OpenXRSwapchain::renderBuffer() const
    {
        return impl().renderBuffer();
    }
}
