#include "directx.hpp"
#ifdef _WIN32

#include <Windows.h>
#include <objbase.h>

#include <d3d11.h>
#include <dxgi1_2.h>

#include <gl/GL.h>

#include <stdexcept>

#include <osg/GraphicsContext>

#include <components/debug/debuglog.hpp>

namespace VR
{

    struct DirectXWGLInteropPrivate
    {
        DirectXWGLInteropPrivate();
        ~DirectXWGLInteropPrivate();

        typedef BOOL(WINAPI* P_wglDXSetResourceShareHandleNV)(void* dxObject, HANDLE shareHandle);
        typedef HANDLE(WINAPI* P_wglDXOpenDeviceNV)(void* dxDevice);
        typedef BOOL(WINAPI* P_wglDXCloseDeviceNV)(HANDLE hDevice);
        typedef HANDLE(WINAPI* P_wglDXRegisterObjectNV)(HANDLE hDevice, void* dxObject,
            GLuint name, GLenum type, GLenum access);
        typedef BOOL(WINAPI* P_wglDXUnregisterObjectNV)(HANDLE hDevice, HANDLE hObject);
        typedef BOOL(WINAPI* P_wglDXObjectAccessNV)(HANDLE hObject, GLenum access);
        typedef BOOL(WINAPI* P_wglDXLockObjectsNV)(HANDLE hDevice, GLint count, HANDLE* hObjects);
        typedef BOOL(WINAPI* P_wglDXUnlockObjectsNV)(HANDLE hDevice, GLint count, HANDLE* hObjects);

        bool mWGL_NV_DX_interop2 = false;
        ID3D11Device* mD3D11Device = nullptr;
        ID3D11DeviceContext* mD3D11ImmediateContext = nullptr;
        HMODULE mD3D11Dll = nullptr;
        PFN_D3D11_CREATE_DEVICE pD3D11CreateDevice = nullptr;

        P_wglDXSetResourceShareHandleNV wglDXSetResourceShareHandleNV = nullptr;
        P_wglDXOpenDeviceNV wglDXOpenDeviceNV = nullptr;
        P_wglDXCloseDeviceNV wglDXCloseDeviceNV = nullptr;
        P_wglDXRegisterObjectNV wglDXRegisterObjectNV = nullptr;
        P_wglDXUnregisterObjectNV wglDXUnregisterObjectNV = nullptr;
        P_wglDXObjectAccessNV wglDXObjectAccessNV = nullptr;
        P_wglDXLockObjectsNV wglDXLockObjectsNV = nullptr;
        P_wglDXUnlockObjectsNV wglDXUnlockObjectsNV = nullptr;

        HANDLE wglDXDevice = nullptr;
    };

    class DirectXSharedImage
    {
    public:
        DirectXSharedImage(uint64_t image, uint32_t glTextureTarget, DXGI_FORMAT format, std::shared_ptr<DirectXWGLInterop> wglInterop);
        ~DirectXSharedImage();

        uint64_t beginFrame(osg::GraphicsContext* gc);
        void endFrame(osg::GraphicsContext* gc);

    protected:
        std::shared_ptr<DirectXWGLInterop> mWglInterop;
        ID3D11Device* mDevice = nullptr;
        ID3D11DeviceContext* mDeviceContext = nullptr;
        ID3D11Texture2D* mSharedTexture = nullptr;
        uint32_t mGlTextureName = 0;
        void* mDxResourceShareHandle = nullptr;

        ID3D11Texture2D* mDxImage;
    };

    DirectXSwapchain::DirectXSwapchain(std::shared_ptr<Swapchain> swapchain, std::shared_ptr<DirectXWGLInterop> wglInterop)
        : Swapchain(*swapchain)
        , mSwapchain(swapchain)
        , mWglInterop(wglInterop)
        , mSharedImages()
        , mCurrentImage(0)
    {
        mMustFlipVertical = true;
    }

    DirectXSwapchain::~DirectXSwapchain()
    {
    }

    uint64_t DirectXSwapchain::beginFrame(osg::GraphicsContext* gc)
    {
        mCurrentImage = mSwapchain->beginFrame(gc);

        auto it = mSharedImages.find(mCurrentImage);
        if (it == mSharedImages.end())
        {
            it = mSharedImages.emplace(mCurrentImage, std::make_unique<DirectXSharedImage>(mCurrentImage, textureTarget(), static_cast<DXGI_FORMAT>(mSwapchain->format()), mWglInterop)).first;
        }

        return mImage = it->second->beginFrame(gc);
    }

    void DirectXSwapchain::endFrame(osg::GraphicsContext* gc)
    {
        auto it = mSharedImages.find(mCurrentImage);
        if (it == mSharedImages.end())
        {
            throw std::logic_error("Missing directx shared image");
        }

        it->second->endFrame(gc);
        mSwapchain->endFrame(gc);
        mImage = 0;
    }

    DirectXSharedImage::DirectXSharedImage(uint64_t image, uint32_t glTextureTarget, DXGI_FORMAT format, std::shared_ptr<DirectXWGLInterop> wglInterop)
        : mWglInterop(wglInterop)
        , mDxImage(reinterpret_cast<ID3D11Texture2D*>(image))
    {
        mDxImage->GetDevice(&mDevice);
        mDevice->GetImmediateContext(&mDeviceContext);

        glGenTextures(1, &mGlTextureName);

        mDxResourceShareHandle = mWglInterop->DXRegisterObject(mDxImage, mGlTextureName, glTextureTarget, true, nullptr);

        if (!mDxResourceShareHandle)
        {
            // Some OpenXR runtimes return textures that cannot be directly shared.
            // So we need to make a second texture to use as an intermediary.
            D3D11_TEXTURE2D_DESC desc{};
            mDxImage->GetDesc(&desc);
            D3D11_TEXTURE2D_DESC sharedTextureDesc{};
            sharedTextureDesc.Width = desc.Width;
            sharedTextureDesc.Height = desc.Height;
            sharedTextureDesc.MipLevels = desc.MipLevels;
            sharedTextureDesc.ArraySize = desc.ArraySize;
            sharedTextureDesc.Format = static_cast<DXGI_FORMAT>(format);
            sharedTextureDesc.SampleDesc = desc.SampleDesc;
            sharedTextureDesc.Usage = D3D11_USAGE_DEFAULT;
            sharedTextureDesc.BindFlags = 0;
            sharedTextureDesc.CPUAccessFlags = 0;
            sharedTextureDesc.MiscFlags = 0;;

            //mDevice->CreateTexture2D(&sharedTextureDesc, nullptr, &mSharedTexture);
            mDevice->CreateTexture2D(&sharedTextureDesc, nullptr, &mSharedTexture);

            mDxResourceShareHandle = mWglInterop->DXRegisterObject(mSharedTexture, mGlTextureName, glTextureTarget, true, nullptr);
        }
    }

    DirectXSharedImage::~DirectXSharedImage()
    {
        mWglInterop->DXUnregisterObject(mDxResourceShareHandle);
    }

    uint64_t DirectXSharedImage::beginFrame(osg::GraphicsContext* gc)
    {
        if(!mWglInterop->DXLockObject(mDxResourceShareHandle))
            Log(Debug::Verbose) << "DXLockObject failed";
        return mGlTextureName;
    }

    void DirectXSharedImage::endFrame(osg::GraphicsContext* gc)
    {
        if(!mWglInterop->DXUnlockObject(mDxResourceShareHandle))
            Log(Debug::Verbose) << "DXUnlockObject failed";
        if (mSharedTexture)
        {
            mDeviceContext->CopyResource(mDxImage, mSharedTexture);
        }
    }

    DirectXWGLInteropPrivate::DirectXWGLInteropPrivate()
    {
        mD3D11Dll = LoadLibrary("D3D11.dll");

        if (!mD3D11Dll)
            throw std::runtime_error("Current OpenXR runtime requires DirectX >= 11.0 but D3D11.dll was not found.");

        pD3D11CreateDevice = reinterpret_cast<PFN_D3D11_CREATE_DEVICE>(GetProcAddress(mD3D11Dll, "D3D11CreateDevice"));

        if (!pD3D11CreateDevice)
            throw std::runtime_error("Symbol 'D3D11CreateDevice' not found in D3D11.dll");

        // Create the device and device context objects
        pD3D11CreateDevice(
            nullptr,
            D3D_DRIVER_TYPE_HARDWARE,
            nullptr,
            0,
            nullptr,
            0,
            D3D11_SDK_VERSION,
            &mD3D11Device,
            nullptr,
            &mD3D11ImmediateContext);

#define LOAD_WGL(a) a = reinterpret_cast<decltype(a)>(wglGetProcAddress(#a)); if(!a) throw std::runtime_error("Extension WGL_NV_DX_interop2 required to run OpenMW VR via DirectX missing expected symbol '" #a "'.")
        LOAD_WGL(wglDXSetResourceShareHandleNV);
        LOAD_WGL(wglDXOpenDeviceNV);
        LOAD_WGL(wglDXCloseDeviceNV);
        LOAD_WGL(wglDXRegisterObjectNV);
        LOAD_WGL(wglDXUnregisterObjectNV);
        LOAD_WGL(wglDXObjectAccessNV);
        LOAD_WGL(wglDXLockObjectsNV);
        LOAD_WGL(wglDXUnlockObjectsNV);
#undef LOAD_WGL

        wglDXDevice = wglDXOpenDeviceNV(mD3D11Device);
    }

    DirectXWGLInteropPrivate::~DirectXWGLInteropPrivate()
    {
    }

    DirectXWGLInterop::DirectXWGLInterop()
        : mPrivate(std::make_unique<DirectXWGLInteropPrivate>())
    {
    }

    DirectXWGLInterop::~DirectXWGLInterop()
    {
    }

    void* DirectXWGLInterop::DXRegisterObject(void* dxResource, uint32_t glName, uint32_t glType, bool discard, void* ntShareHandle)
    {
        if (ntShareHandle)
        {
            mPrivate->wglDXSetResourceShareHandleNV(dxResource, ntShareHandle);
        }
        return mPrivate->wglDXRegisterObjectNV(mPrivate->wglDXDevice, dxResource, glName, glType, 1);
    }

    void DirectXWGLInterop::DXUnregisterObject(void* dxResourceShareHandle)
    {
        mPrivate->wglDXUnregisterObjectNV(mPrivate->wglDXDevice, dxResourceShareHandle);
    }

    bool DirectXWGLInterop::DXLockObject(void* dxResourceShareHandle)
    {
        return !!mPrivate->wglDXLockObjectsNV(mPrivate->wglDXDevice, 1, &dxResourceShareHandle);
    }

    bool DirectXWGLInterop::DXUnlockObject(void* dxResourceShareHandle)
    {
        return !!mPrivate->wglDXUnlockObjectsNV(mPrivate->wglDXDevice, 1, &dxResourceShareHandle);
    }

    void* DirectXWGLInterop::d3d11DeviceHandle()
    {
        return mPrivate->mD3D11Device;
    }
}
#endif
