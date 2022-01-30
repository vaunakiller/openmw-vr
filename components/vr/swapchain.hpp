#ifndef VR_SWAPCHAIN_H
#define VR_SWAPCHAIN_H

#include <cstdint>

namespace osg
{
    class GraphicsContext;
}

namespace VR
{
    struct SwapchainConfig
    {
        uint32_t recommendedWidth = 0;
        uint32_t recommendedHeight = 0;
        uint32_t recommendedSamples = 0;
        uint32_t maxWidth = 0;
        uint32_t maxHeight = 0;
        uint32_t maxSamples = 0;
    };

    class Swapchain
    {
    public:
        Swapchain(uint32_t width, uint32_t height, uint32_t samples, uint32_t format, uint32_t arraySize, uint32_t textureTarget, uint32_t size, bool mustFlipVertical);

        virtual ~Swapchain() {};

        //! Acquire a rendering surface from this swapchain
        virtual uint64_t beginFrame(osg::GraphicsContext* gc) = 0;

        //! Release the rendering surface
        virtual void endFrame(osg::GraphicsContext* gc) = 0;

        //! Width of the surface
        uint32_t width() const { return mWidth; };

        //! Height of the surface
        uint32_t height() const { return mHeight; };

        //! Samples of the surface
        uint32_t samples() const { return mSamples; };

        //! Pixel format of the surface
        uint32_t format() const { return mFormat; };

        //! Array depth of the surface
        uint32_t arraySize() const { return mArraySize; };

        //! Array depth of the surface
        uint32_t textureTarget() const { return mTextureTarget; };

        //! Get the currently active opengl image
        uint64_t image() const { return mImage; };

        //! The number of images in the swapchain
        uint32_t size() const { return mSize; };

        //! Underlying handle
        virtual void* handle() const = 0;

        //! Whether or not the image must be flipped vertically when drawing into the swapchain
        bool mustFlipVertical() const { return mMustFlipVertical; };

    protected:
        uint32_t mWidth;
        uint32_t mHeight;
        uint32_t mSamples;
        uint32_t mFormat;
        uint32_t mArraySize;
        uint32_t mTextureTarget;
        uint32_t mSize;
        bool mMustFlipVertical;

        uint64_t mImage;

    private:
    };
}

#endif
