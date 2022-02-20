#ifndef VR_DIRECTX_H
#define VR_DIRECTX_H

#ifdef _WIN32
// This header contains classes and methods for sharing directx surfaces with OpenGL
// for use on platforms that offer directx surfaces but not opengl surfaces

#include <cstdint>
#include <memory>
#include <map>

#include "swapchain.hpp"

namespace osg
{
    class GraphicsContext;
}

namespace VR
{
    class DirectXSharedImage;
    struct DirectXWGLInteropPrivate;

    class DirectXWGLInterop
    {
    public:
        DirectXWGLInterop();
        ~DirectXWGLInterop();

        /// Registers an object for sharing as if calling wglDXRegisterObjectNV requesting write access.
        /// If ntShareHandle is not null, wglDXSetResourceShareHandleNV is called first to register the share handle
        void* DXRegisterObject(void* dxResource, uint32_t glName, uint32_t glType, bool discard, void* ntShareHandle);
        /// Unregisters an object from sharing as if calling wglDXUnregisterObjectNV
        void  DXUnregisterObject(void* dxResourceShareHandle);
        /// Locks a DX object for use by OpenGL as if calling wglDXLockObjectsNV
        bool  DXLockObject(void* dxResourceShareHandle);
        /// Unlocks a DX object for use by DirectX as if calling wglDXUnlockObjectsNV
        bool  DXUnlockObject(void* dxResourceShareHandle);

        void* d3d11DeviceHandle();

        std::unique_ptr<DirectXWGLInteropPrivate> mPrivate;
    };

    class DirectXSwapchain : public Swapchain
    {
    public:
        DirectXSwapchain(std::shared_ptr<Swapchain> dxSwapchain, std::shared_ptr<DirectXWGLInterop> wglInterop, uint32_t openGLFormat);
        virtual ~DirectXSwapchain();

        //! Acquire directx surface from underlying swapchain and share it with opengl, then returns an opengl surface
        uint64_t beginFrame(osg::GraphicsContext* gc) override;

        //! Release the rendering surface
        void endFrame(osg::GraphicsContext* gc) override;

        //! Fetch handle of underlying swapchain
        void* handle() const override { return mDXSwapchain->handle(); };

    protected:
        std::shared_ptr<Swapchain> mDXSwapchain;
        std::shared_ptr<DirectXWGLInterop> mWglInterop;
        std::map<uint64_t, std::unique_ptr<DirectXSharedImage>> mSharedImages;
        uint64_t mCurrentImage;

    private:
    };
}

#else
namespace VR
{
    class DirectXWGLInterop
    {};
}
#endif

#endif
