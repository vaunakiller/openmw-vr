#ifndef OPENXR_SWAPCHAINIMPL_HPP
#define OPENXR_SWAPCHAINIMPL_HPP

#include "openxrswapchain.hpp"
#include "openxrmanagerimpl.hpp"

struct XrSwapchainSubImage;

namespace MWVR
{

class OpenXRSwapchainImpl
{
public:
    OpenXRSwapchainImpl(osg::ref_ptr<osg::State> state, SwapchainConfig config);
    ~OpenXRSwapchainImpl();

    void beginFrame(osg::GraphicsContext* gc);
    void endFrame(osg::GraphicsContext* gc);
    void acquire(osg::GraphicsContext* gc);
    void release(osg::GraphicsContext* gc);

    VRTexture* renderBuffer() const;

    uint32_t acquiredImage() const;

    bool isAcquired() const;
    XrSwapchain xrSwapchain(void) const { return mSwapchain; };
    XrSwapchainSubImage xrSubImage(void) const { return mSubImage; };
    int width() const { return mWidth; };
    int height() const { return mHeight; };
    int samples() const { return mSamples; };

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
}

#endif
