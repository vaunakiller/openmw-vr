#ifndef OPENXR_SWAPCHAINIMPL_HPP
#define OPENXR_SWAPCHAINIMPL_HPP

#include "openxrswapchain.hpp"
#include "openxrmanagerimpl.hpp"

struct XrSwapchainSubImage;
struct XrSwapchainImageOpenGLKHR;

namespace MWVR
{
    /// \brief Implementation of OpenXRSwapchain
    class OpenXRSwapchainImpl
    {
    private:
        struct SwapchainPrivate
        {
            enum class Use {
                COLOR = 0,
                DEPTH = 1
            };

            SwapchainPrivate(osg::ref_ptr<osg::State> state, SwapchainConfig config, Use use);
            ~SwapchainPrivate();

            uint32_t bufferAt(uint32_t index) const;
            uint32_t count() const;
            uint32_t acuiredBuffer() const;
            uint32_t acuiredIndex() const { return mAcquiredIndex; };
            bool isAcquired() const;
            XrSwapchain xrSwapchain(void) const { return mSwapchain; };
            XrSwapchainSubImage xrSubImage(void) const { return mSubImage; };
            int width() const { return mWidth; };
            int height() const { return mHeight; };
            int samples() const { return mSamples; };

            void acquire();
            void release();
            void checkAcquired() const;

        private:
            XrSwapchain mSwapchain = XR_NULL_HANDLE;
            XrSwapchainSubImage mSubImage{};
            std::vector<XrSwapchainImageOpenGLKHR> mBuffers;
            int32_t mWidth = -1;
            int32_t mHeight = -1;
            int32_t mSamples = -1;
            int64_t mFormat = -1;
            uint32_t mAcquiredIndex{ 0 };
            bool mIsIndexAcquired{ false };
            bool mIsReady{ false };
        };

    public:
        OpenXRSwapchainImpl(osg::ref_ptr<osg::State> state, SwapchainConfig config);
        ~OpenXRSwapchainImpl();

        void beginFrame(osg::GraphicsContext* gc);
        void endFrame(osg::GraphicsContext* gc);

        VRFramebuffer* renderBuffer() const;
        uint32_t acquiredColorTexture() const;
        uint32_t acquiredDepthTexture() const;

        bool isAcquired() const;
        XrSwapchain xrSwapchain(void) const { return mSwapchain->xrSwapchain(); };
        XrSwapchain xrSwapchainDepth(void) const { return mSwapchainDepth->xrSwapchain(); };
        XrSwapchainSubImage xrSubImage(void) const { return mSwapchain->xrSubImage(); };
        XrSwapchainSubImage xrSubImageDepth(void) const { return mSwapchainDepth->xrSubImage(); };
        int width() const { return mConfig.selectedWidth; };
        int height() const { return mConfig.selectedHeight; };
        int samples() const { return mConfig.selectedSamples; };

    protected:
        OpenXRSwapchainImpl(const OpenXRSwapchainImpl&) = delete;
        void operator=(const OpenXRSwapchainImpl&) = delete;

        void acquire();
        void release();
        void checkAcquired() const;

    private:
        SwapchainConfig mConfig;
        std::unique_ptr<SwapchainPrivate> mSwapchain{ nullptr };
        std::unique_ptr<SwapchainPrivate> mSwapchainDepth{ nullptr };
        std::vector<std::unique_ptr<VRFramebuffer> > mRenderBuffers{};
        bool mFormallyAcquired{ false };
        bool mShouldRelease{ false };
    };
}

#endif
