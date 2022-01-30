#include "frame.hpp"
#include "swapchain.hpp"

namespace VR
{
    Swapchain::Swapchain(uint32_t width, uint32_t height, uint32_t samples, uint32_t format, uint32_t arraySize, uint32_t textureTarget, uint32_t size, bool mustFlipVertical)
        : mWidth(width)
        , mHeight(height)
        , mSamples(samples)
        , mFormat(format)
        , mArraySize(arraySize)
        , mTextureTarget(textureTarget)
        , mSize(textureTarget)
        , mMustFlipVertical(mustFlipVertical)
        , mImage(0)
    {
    }
}
