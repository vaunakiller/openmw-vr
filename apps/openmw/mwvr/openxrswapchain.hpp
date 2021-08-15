#ifndef OPENXR_SWAPCHAIN_HPP
#define OPENXR_SWAPCHAIN_HPP

#include "openxrmanager.hpp"

#include <components/vr/swapchain.hpp>
#include <openxr/openxr.h>

struct XrSwapchainSubImage;

namespace MWVR
{
    class OpenXRSwapchainImpl;
    class VRFramebuffer;

    class OpenXRSwapchain : public VR::Swapchain
    {
    public:
        OpenXRSwapchain(XrSwapchain swapchain, std::vector<uint64_t> images, uint32_t width, uint32_t height, uint32_t samples, uint32_t format);
        ~OpenXRSwapchain() override;

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
