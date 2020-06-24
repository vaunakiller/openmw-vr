#ifndef OPENXR_SWAPCHAIN_HPP
#define OPENXR_SWAPCHAIN_HPP

#include "openxrmanager.hpp"
#include "vrtexture.hpp"

struct XrSwapchainSubImage;

namespace MWVR
{
    class OpenXRSwapchainImpl;

    class OpenXRSwapchain
    {
    public:
        struct Config
        {
            int width = -1;
            int height = -1;
            int samples = -1;
        };

    public:
        OpenXRSwapchain(osg::ref_ptr<osg::State> state, SwapchainConfig config);
        ~OpenXRSwapchain();

    public:
        //! Prepare for render (set FBO)
        void beginFrame(osg::GraphicsContext* gc);
        //! Finalize render
        void endFrame(osg::GraphicsContext* gc);
        //! Prepare for render
        void acquire(osg::GraphicsContext* gc);
        //! Finalize render
        void release(osg::GraphicsContext* gc);
        //! Currently acquired image
        uint32_t acquiredImage() const;
        //! Whether subchain is currently acquired (true) or released (false)
        bool isAcquired() const;
        //! Width of the view surface
        int width() const;
        //! Height of the view surface
        int height() const;
        //! Samples of the view surface
        int samples() const;
        //! Get the current texture
        VRTexture* renderBuffer() const;
        //! Get the private implementation
        OpenXRSwapchainImpl& impl() { return *mPrivate; }
        //! Get the private implementation
        const OpenXRSwapchainImpl& impl() const { return *mPrivate; }

    private:
        std::unique_ptr<OpenXRSwapchainImpl> mPrivate;
    };
}

#endif
