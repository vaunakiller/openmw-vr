#ifndef VR_SWAPCHAIN_H
#define VR_SWAPCHAIN_H

#include <cstdint>

namespace osg
{
    class GraphicsContext;
}

namespace VR
{
    class Swapchain
    {
    public:
        Swapchain(uint32_t width, uint32_t height, uint32_t samples, uint32_t format, bool mustFlipVertical);
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

        //! Underlying handle
        virtual void* handle() const = 0;

        //! Whether or not the image must be flipped vertically when drawing into the swapchain
        bool mustFlipVertical() const { return mMustFlipVertical; };

    protected:
        uint32_t mWidth;
        uint32_t mHeight;
        uint32_t mSamples;
        uint32_t mFormat;
        bool mMustFlipVertical;

    private:
    };
}

#endif
