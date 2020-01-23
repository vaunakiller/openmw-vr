#include "openxrswapchain.hpp"
#include "openxrmanager.hpp"
#include "openxrmanagerimpl.hpp"

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

        OpenXRSwapchainImpl(osg::ref_ptr<OpenXRManager> XR, osg::ref_ptr<osg::State> state, OpenXRSwapchain::Config config);
        ~OpenXRSwapchainImpl();

        osg::ref_ptr<OpenXRTextureBuffer> prepareNextSwapchainImage();
        void   releaseSwapchainImage();
        void beginFrame(osg::GraphicsContext* gc);
        void endFrame(osg::GraphicsContext* gc);

        osg::ref_ptr<OpenXRManager> mXR;
        XrSwapchain mSwapchain = XR_NULL_HANDLE;
        std::vector<XrSwapchainImageOpenGLKHR> mSwapchainImageBuffers{};
        std::vector<osg::ref_ptr<OpenXRTextureBuffer> > mTextureBuffers{};
        XrSwapchainSubImage mSubImage{};
        int32_t mWidth = -1;
        int32_t mHeight = -1;
        int32_t mSamples = -1;
        int64_t mSwapchainColorFormat = -1;
        OpenXRTextureBuffer* mCurrentBuffer = nullptr;
    };

    OpenXRSwapchainImpl::OpenXRSwapchainImpl(
        osg::ref_ptr<OpenXRManager> XR, osg::ref_ptr<osg::State> state, OpenXRSwapchain::Config config)
        : mXR(XR)
        , mWidth(config.width)
        , mHeight(config.height)
        , mSamples(config.samples)
    {
        if (mWidth <= 0)
            throw std::invalid_argument("Width must be a positive integer");
        if (mHeight <= 0)
            throw std::invalid_argument("Height must be a positive integer");
        if (mSamples <= 0)
            throw std::invalid_argument("Samples must be a positive integer");

        auto& pXR = mXR->impl();

        // Select a swapchain format.
        uint32_t swapchainFormatCount;
        CHECK_XRCMD(xrEnumerateSwapchainFormats(pXR.mSession, 0, &swapchainFormatCount, nullptr));
        std::vector<int64_t> swapchainFormats(swapchainFormatCount);
        CHECK_XRCMD(xrEnumerateSwapchainFormats(pXR.mSession, (uint32_t)swapchainFormats.size(), &swapchainFormatCount, swapchainFormats.data()));

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
        CHECK_XRCMD(xrCreateSwapchain(pXR.mSession, &swapchainCreateInfo, &mSwapchain));

        uint32_t imageCount = 0;
        CHECK_XRCMD(xrEnumerateSwapchainImages(mSwapchain, 0, &imageCount, nullptr));

        mSwapchainImageBuffers.resize(imageCount, { XR_TYPE_SWAPCHAIN_IMAGE_OPENGL_KHR });
        CHECK_XRCMD(xrEnumerateSwapchainImages(mSwapchain, imageCount, &imageCount, reinterpret_cast<XrSwapchainImageBaseHeader*>(mSwapchainImageBuffers.data())));
        for (const auto& swapchainImage : mSwapchainImageBuffers)
            mTextureBuffers.push_back(new OpenXRTextureBuffer(state, swapchainImage.image, mWidth, mHeight, 0));

        mSubImage.swapchain = mSwapchain;
        mSubImage.imageRect.offset = { 0, 0 };
        mSubImage.imageRect.extent = { mWidth, mHeight };
    }

    OpenXRSwapchainImpl::~OpenXRSwapchainImpl()
    {
        if (mSwapchain)
            CHECK_XRCMD(xrDestroySwapchain(mSwapchain));
    }

    osg::ref_ptr<OpenXRTextureBuffer>
        OpenXRSwapchainImpl::prepareNextSwapchainImage()
    {
        if (!mXR->sessionRunning())
            return nullptr;

        XrSwapchainImageAcquireInfo acquireInfo{ XR_TYPE_SWAPCHAIN_IMAGE_ACQUIRE_INFO };
        uint32_t swapchainImageIndex = 0;
        CHECK_XRCMD(xrAcquireSwapchainImage(mSwapchain, &acquireInfo, &swapchainImageIndex));

        XrSwapchainImageWaitInfo waitInfo{ XR_TYPE_SWAPCHAIN_IMAGE_WAIT_INFO };
        waitInfo.timeout = XR_INFINITE_DURATION;
        CHECK_XRCMD(xrWaitSwapchainImage(mSwapchain, &waitInfo));

        return mTextureBuffers[swapchainImageIndex];
    }

    void
        OpenXRSwapchainImpl::releaseSwapchainImage()
    {
        if (!mXR->sessionRunning())
            return;

        XrSwapchainImageReleaseInfo releaseInfo{ XR_TYPE_SWAPCHAIN_IMAGE_RELEASE_INFO };
        CHECK_XRCMD(xrReleaseSwapchainImage(mSwapchain, &releaseInfo));
        //mCurrentBuffer = nullptr;
    }

    void OpenXRSwapchainImpl::beginFrame(osg::GraphicsContext* gc)
    {
        mCurrentBuffer = prepareNextSwapchainImage();
        if (mCurrentBuffer)
            mCurrentBuffer->beginFrame(gc);
    }

    void OpenXRSwapchainImpl::endFrame(osg::GraphicsContext* gc)
    {
        if (mCurrentBuffer)
            mCurrentBuffer->endFrame(gc);
        releaseSwapchainImage();
    }

    OpenXRSwapchain::OpenXRSwapchain(
        osg::ref_ptr<OpenXRManager> XR, osg::ref_ptr<osg::State> state, OpenXRSwapchain::Config config)
        : mPrivate(new OpenXRSwapchainImpl(XR, state, config))
    {
    }

    OpenXRSwapchain::~OpenXRSwapchain()
    {
    }

    osg::ref_ptr<OpenXRTextureBuffer> OpenXRSwapchain::prepareNextSwapchainImage()
    {
        return impl().prepareNextSwapchainImage();
    }

    void OpenXRSwapchain::releaseSwapchainImage()
    {
        impl().releaseSwapchainImage();
    }

    void OpenXRSwapchain::beginFrame(osg::GraphicsContext* gc)
    {
        impl().beginFrame(gc);
    }

    void OpenXRSwapchain::endFrame(osg::GraphicsContext* gc)
    {
        impl().endFrame(gc);
    }

    const XrSwapchainSubImage& 
        OpenXRSwapchain::subImage(void) const
    {
        return impl().mSubImage;
    }

    int OpenXRSwapchain::width()
    {
        return impl().mWidth;
    }

    int OpenXRSwapchain::height()
    {
        return impl().mHeight;
    }

    int OpenXRSwapchain::samples()
    {
        return impl().mSamples;
    }

    OpenXRTextureBuffer* OpenXRSwapchain::current()
    {
        return impl().mCurrentBuffer;
    }
}
