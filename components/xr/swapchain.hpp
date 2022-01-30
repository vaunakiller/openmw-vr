#ifndef OPENXR_SWAPCHAIN_HPP
#define OPENXR_SWAPCHAIN_HPP

#include "instance.hpp"
#include <components/vr/swapchain.hpp>
#include <openxr/openxr.h>

struct XrSwapchainSubImage;

namespace XR
{
    class OpenXRSwapchainImpl;
    class VRFramebuffer;

    class Swapchain : public VR::Swapchain
    {
    public:
        Swapchain(XrSwapchain swapchain, std::vector<uint64_t> images, uint32_t width, uint32_t height, uint32_t samples, uint32_t format, uint32_t arraySize, uint32_t textureTarget);
        ~Swapchain() override;

        XrSwapchain xrSwapchain() const { return mXrSwapchain; };

    protected:
        //! Acquire a rendering surface from this swapchain
        uint64_t beginFrame(osg::GraphicsContext* gc) override;

        //! Release the rendering surface
        void endFrame(osg::GraphicsContext* gc) override;

        //! Return the xr swapchain
        void* handle() const override { return mXrSwapchain; };

    private:
        void acquire();
        void wait();
        void release();
        bool isAcquired();

        uint32_t mAcquiredIndex = 0;
        bool mIsAcquired = false;
        bool mIsReady = false;
        XrSwapchain mXrSwapchain;
        std::vector<uint64_t> mImages;
    };
}

#endif
