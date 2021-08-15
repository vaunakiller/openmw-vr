#include "frame.hpp"
#include "swapchain.hpp"

namespace VR
{
    Swapchain::Swapchain(uint32_t width, uint32_t height, uint32_t samples, uint32_t format, bool mustFlipVertical)
        : mWidth(width)
        , mHeight(height)
        , mSamples(samples)
        , mFormat(format)
        , mMustFlipVertical(mustFlipVertical)
    {
    }
}
