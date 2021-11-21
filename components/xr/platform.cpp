#include "instance.hpp"
#include "platform.hpp"
#include "session.hpp"
#include "debug.hpp"
#include "swapchain.hpp"

// The OpenXR SDK's platform headers assume we've included platform headers
#ifdef _WIN32
#include <Windows.h>
#include <objbase.h>

#ifdef XR_USE_GRAPHICS_API_D3D11
#include <d3d11_1.h>
#endif

#elif __linux__
#include <GL/glx.h>
#undef None

#else
#error Unsupported platform
#endif

#include <openxr/openxr.h>
#include <openxr/openxr_platform.h>
#include <openxr/openxr_platform_defines.h>
#include <openxr/openxr_reflection.h>

#include <stdexcept>
#include <deque>
#include <cassert>
#include <optional>

namespace XR
{
    struct PlatformPrivate
    {
        PlatformPrivate(osg::GraphicsContext* gc);
        ~PlatformPrivate();

#ifdef XR_USE_GRAPHICS_API_D3D11
        bool mWGL_NV_DX_interop2 = false;
#endif
    };

    PlatformPrivate::PlatformPrivate(osg::GraphicsContext* gc)
    {
#ifdef XR_USE_GRAPHICS_API_D3D11
        typedef const char* (WINAPI* PFNWGLGETEXTENSIONSSTRINGARBPROC) (HDC hdc);
        PFNWGLGETEXTENSIONSSTRINGARBPROC wglGetExtensionsStringARB = 0;
        wglGetExtensionsStringARB = reinterpret_cast<PFNWGLGETEXTENSIONSSTRINGARBPROC>(wglGetProcAddress("wglGetExtensionsStringARB"));
        if (wglGetExtensionsStringARB)
        {
            std::string wglExtensions = wglGetExtensionsStringARB(wglGetCurrentDC());
            Log(Debug::Verbose) << "WGL Extensions: " << wglExtensions;
            mWGL_NV_DX_interop2 = wglExtensions.find("WGL_NV_DX_interop2") != std::string::npos;
        }
        else
            Log(Debug::Verbose) << "Unable to query WGL extensions";
#endif
    }

    PlatformPrivate::~PlatformPrivate()
    {
    }

    Platform::Platform(osg::GraphicsContext* gc)
        : mPrivate(new PlatformPrivate(gc))
    {
    }

    Platform::~Platform()
    {
    }

    bool Platform::selectDirectX()
    {
#ifdef XR_USE_GRAPHICS_API_D3D11
        if (mPrivate->mWGL_NV_DX_interop2)
        {
            if (KHR_D3D11_enable.supported())
            {
                Extensions::instance().selectGraphicsAPIExtension(&KHR_D3D11_enable);
                return true;
            }
            else
                Log(Debug::Warning) << "Warning: Failed to select DirectX swapchains: OpenXR runtime does not support essential extension '" << KHR_D3D11_enable.name() << "'";
        }
        else
            Log(Debug::Warning) << "Warning: Failed to select DirectX swapchains: Essential WGL extension 'WGL_NV_DX_interop2' not supported by the graphics driver.";
#endif
        return false;
    }

    bool Platform::selectOpenGL()
    {
#ifdef XR_USE_GRAPHICS_API_OPENGL
        if (KHR_opengl_enable.supported())
        {
            Extensions::instance().selectGraphicsAPIExtension(&KHR_opengl_enable);
            return true;
        }
        else
            Log(Debug::Warning) << "Warning: Failed to select OpenGL swapchains: OpenXR runtime does not support essential extension '" << KHR_opengl_enable.name() << "'";
#endif
        return false;
    }

    void Platform::selectGraphicsAPIExtension()
    {
        bool preferDirectX = Settings::Manager::getBool("Prefer DirectX swapchains", "VR");

        if (preferDirectX)
            if (selectDirectX() || selectOpenGL())
                return;
        if (selectOpenGL() || selectDirectX())
            return;

        Log(Debug::Verbose) << "Error: No graphics API supported by OpenMW VR is supported by the OpenXR runtime.";
        throw std::runtime_error("Error: No graphics API supported by OpenMW VR is supported by the OpenXR runtime.");
    }

    XrSession Platform::createXrSession(XrInstance instance, XrSystemId systemId)
    {
        XrSession session = XR_NULL_HANDLE;
        XrResult res = XR_SUCCESS;
#ifdef _WIN32
        if(KHR_opengl_enable.enabled())
        { 
            // Get system requirements
            PFN_xrGetOpenGLGraphicsRequirementsKHR p_getRequirements = nullptr;
            CHECK_XRCMD(xrGetInstanceProcAddr(instance, "xrGetOpenGLGraphicsRequirementsKHR", reinterpret_cast<PFN_xrVoidFunction*>(&p_getRequirements)));
            XrGraphicsRequirementsOpenGLKHR requirements{};
            requirements.type = XR_TYPE_GRAPHICS_REQUIREMENTS_OPENGL_KHR;
            CHECK_XRCMD(p_getRequirements(instance, systemId, &requirements));

            // TODO: Actually get system version
            const XrVersion systemApiVersion = XR_MAKE_VERSION(4, 6, 0);
            if (requirements.minApiVersionSupported > systemApiVersion) {
                std::cout << "Runtime does not support desired Graphics API and/or version" << std::endl;
            }

            // Create Session
            auto DC = wglGetCurrentDC();
            auto GLRC = wglGetCurrentContext();

            XrGraphicsBindingOpenGLWin32KHR graphicsBindings{};
            graphicsBindings.type = XR_TYPE_GRAPHICS_BINDING_OPENGL_WIN32_KHR;
            graphicsBindings.hDC = DC;
            graphicsBindings.hGLRC = GLRC;

            if (!graphicsBindings.hDC)
                Log(Debug::Warning) << "Missing DC";

            XrSessionCreateInfo createInfo{};
            createInfo.type = XR_TYPE_SESSION_CREATE_INFO;
            createInfo.next = &graphicsBindings;
            createInfo.systemId = systemId;
            res = CHECK_XRCMD(xrCreateSession(instance, &createInfo, &session));
        }
#ifdef XR_USE_GRAPHICS_API_D3D11
        else if(KHR_D3D11_enable.enabled())
        {
            mDxInterop = std::make_shared<VR::DirectXWGLInterop>();
            PFN_xrGetD3D11GraphicsRequirementsKHR p_getRequirements = nullptr;
            CHECK_XRCMD(xrGetInstanceProcAddr(instance, "xrGetD3D11GraphicsRequirementsKHR", reinterpret_cast<PFN_xrVoidFunction*>(&p_getRequirements)));
            XrGraphicsRequirementsD3D11KHR requirements{};
            requirements.type = XR_TYPE_GRAPHICS_REQUIREMENTS_D3D11_KHR;
            CHECK_XRCMD(p_getRequirements(instance, systemId, &requirements));

            XrGraphicsBindingD3D11KHR d3D11bindings{};
            d3D11bindings.type = XR_TYPE_GRAPHICS_BINDING_D3D11_KHR;
            d3D11bindings.device = reinterpret_cast<ID3D11Device*>(mDxInterop->d3d11DeviceHandle());
            XrSessionCreateInfo createInfo{};
            createInfo.type = XR_TYPE_SESSION_CREATE_INFO;
            createInfo.next = &d3D11bindings;
            createInfo.systemId = systemId;
            res = CHECK_XRCMD(xrCreateSession(instance, &createInfo, &session));
        }
#endif
        else
        {
            throw std::logic_error("Enum value not implemented");
        }
#elif __linux__
        {
            // Get system requirements
            PFN_xrGetOpenGLGraphicsRequirementsKHR p_getRequirements = nullptr;
            xrGetInstanceProcAddr(instance, "xrGetOpenGLGraphicsRequirementsKHR", reinterpret_cast<PFN_xrVoidFunction*>(&p_getRequirements));
            XrGraphicsRequirementsOpenGLKHR requirements{};
            requirements.type = XR_TYPE_GRAPHICS_REQUIREMENTS_OPENGL_KHR;
            CHECK_XRCMD(p_getRequirements(instance, systemId, &requirements));

            // TODO: Actually get system version
            const XrVersion systemApiVersion = XR_MAKE_VERSION(4, 6, 0);
            if (requirements.minApiVersionSupported > systemApiVersion) {
                std::cout << "Runtime does not support desired Graphics API and/or version" << std::endl;
            }

            // Create Session
            GLXContext glxContext = glXGetCurrentContext();
            GLXDrawable glxDrawable = glXGetCurrentDrawable();
            Display* xDisplay = glXGetCurrentDisplay();

            // TODO: runtimes don't actually care (yet)
            GLXFBConfig glxFBConfig = 0;
            uint32_t visualid = 0;

            XrGraphicsBindingOpenGLXlibKHR graphicsBindings{};
            graphicsBindings.type = XR_TYPE_GRAPHICS_BINDING_OPENGL_XLIB_KHR;
            graphicsBindings.xDisplay = xDisplay;
            graphicsBindings.glxContext = glxContext;
            graphicsBindings.glxDrawable = glxDrawable;
            graphicsBindings.glxFBConfig = glxFBConfig;
            graphicsBindings.visualid = visualid;

            if (!graphicsBindings.glxContext)
                Log(Debug::Warning) << "Missing glxContext";

            if (!graphicsBindings.glxDrawable)
                Log(Debug::Warning) << "Missing glxDrawable";

            XrSessionCreateInfo createInfo{};
            createInfo.type = XR_TYPE_SESSION_CREATE_INFO;
            createInfo.next = &graphicsBindings;
            createInfo.systemId = systemId;
            res = CHECK_XRCMD(xrCreateSession(instance, &createInfo, &session));
        }
#endif
        if (!XR_SUCCEEDED(res))
            initFailure(res, instance);

        uint32_t swapchainFormatCount;
        CHECK_XRCMD(xrEnumerateSwapchainFormats(session, 0, &swapchainFormatCount, nullptr));
        mSwapchainFormats.resize(swapchainFormatCount);
        CHECK_XRCMD(xrEnumerateSwapchainFormats(session, (uint32_t)mSwapchainFormats.size(), &swapchainFormatCount, mSwapchainFormats.data()));

        std::stringstream ss;
        ss << "Available Swapchain formats: (" << swapchainFormatCount << ")" << std::endl;

        for (auto format : mSwapchainFormats)
        {
            ss << "  Enum=" << std::dec << format << " (0x=" << std::hex << format << ")" << std::dec << std::endl;
        }

        Log(Debug::Verbose) << ss.str();

        return session;
    }

    /*
    * For reference: These are the DXGI formats offered by WMR when using D3D11:
          Enum=29 //(0x=1d) DXGI_FORMAT_R8G8B8A8_UNORM_SRGB
          Enum=91 //(0x=5b) DXGI_FORMAT_B8G8R8A8_UNORM_SRGB
          Enum=28 //(0x=1c) DXGI_FORMAT_R8G8B8A8_UNORM
          Enum=87 //(0x=57) DXGI_FORMAT_B8G8R8A8_UNORM
          Enum=40 //(0x=28) DXGI_FORMAT_D32_FLOAT
          Enum=20 //(0x=14) DXGI_FORMAT_D32_FLOAT_S8X24_UINT
          Enum=45 //(0x=2d) DXGI_FORMAT_D24_UNORM_S8_UINT
          Enum=55 //(0x=37) DXGI_FORMAT_D16_UNORM
    * And these extra formats are offered by SteamVR:
                0xa , // DXGI_FORMAT_R16G16B16A16_FLOAT
                0x18 , // DXGI_FORMAT_R10G10B10A2_UNORM
    */

    int64_t Platform::selectColorFormat(int64_t preferredFormat)
    {
        if (KHR_opengl_enable.enabled())
        {
            std::vector<int64_t> requestedColorSwapchainFormats;

            if (Settings::Manager::getBool("Prefer sRGB swapchains", "VR"))
            {
                requestedColorSwapchainFormats.push_back(0x8C43); // GL_SRGB8_ALPHA8
                requestedColorSwapchainFormats.push_back(0x8C41); // GL_SRGB8
            }

            requestedColorSwapchainFormats.push_back(0x8058); // GL_RGBA8
            requestedColorSwapchainFormats.push_back(0x8F97); // GL_RGBA8_SNORM
            requestedColorSwapchainFormats.push_back(0x881A); // GL_RGBA16F
            requestedColorSwapchainFormats.push_back(0x881B); // GL_RGB16F
            requestedColorSwapchainFormats.push_back(0x8C3A); // GL_R11F_G11F_B10F

            return selectFormat(preferredFormat, requestedColorSwapchainFormats);
        }
#ifdef XR_USE_GRAPHICS_API_D3D11
        else if (KHR_D3D11_enable.enabled())
        {
            std::vector<int64_t> requestedColorSwapchainFormats;

            if (Settings::Manager::getBool("Prefer sRGB swapchains", "VR"))
            {
                requestedColorSwapchainFormats.push_back(0x1d); // DXGI_FORMAT_R8G8B8A8_UNORM_SRGB
                requestedColorSwapchainFormats.push_back(0x5b); // DXGI_FORMAT_B8G8R8A8_UNORM_SRGB
            }

            requestedColorSwapchainFormats.push_back(0x1c); // DXGI_FORMAT_R8G8B8A8_UNORM
            requestedColorSwapchainFormats.push_back(0x57); // DXGI_FORMAT_B8G8R8A8_UNORM
            requestedColorSwapchainFormats.push_back(0xa); // DXGI_FORMAT_R16G16B16A16_FLOAT
            requestedColorSwapchainFormats.push_back(0x18); // DXGI_FORMAT_R10G10B10A2_UNORM

            return selectFormat(preferredFormat, requestedColorSwapchainFormats);
        }
#endif
        else
        {
            throw std::logic_error("Not implemented");
        }

    }

    int64_t Platform::selectDepthFormat(int64_t preferredFormat)
    {
        if (KHR_opengl_enable.enabled())
        {
            // Find supported depth swapchain format.
            std::vector<int64_t> requestedDepthSwapchainFormats = {
                0x88F0, // GL_DEPTH24_STENCIL8
                0x8CAC, // GL_DEPTH_COMPONENT32F
                0x81A7, // GL_DEPTH_COMPONENT32
                0x8DAB, // GL_DEPTH_COMPONENT32F_NV
                0x8CAD, // GL_DEPTH32_STENCIL8
                0x81A6, // GL_DEPTH_COMPONENT24
                // Need 32bit minimum: // 0x81A5, // GL_DEPTH_COMPONENT16
            };

            return selectFormat(preferredFormat, requestedDepthSwapchainFormats);
        }
#ifdef XR_USE_GRAPHICS_API_D3D11
        else if (KHR_D3D11_enable.enabled())
        {
            // Find supported color swapchain format.
            std::vector<int64_t> requestedDepthSwapchainFormats = {
                0x2d, // DXGI_FORMAT_D24_UNORM_S8_UINT
                0x14, // DXGI_FORMAT_D32_FLOAT_S8X24_UINT
                0x28, // DXGI_FORMAT_D32_FLOAT
                // Need 32bit minimum: 0x37, // DXGI_FORMAT_D16_UNORM
            };
            return selectFormat(preferredFormat, requestedDepthSwapchainFormats);
        }
#endif
        else
        {
            throw std::logic_error("Not implemented");
        }
    }

    void Platform::eraseFormat(int64_t format)
    {
        for (auto it = mSwapchainFormats.begin(); it != mSwapchainFormats.end(); it++)
        {
            if (*it == format)
            {
                mSwapchainFormats.erase(it);
                return;
            }
        }
    }

    int64_t openGLFormatToDirectXFormat(int64_t format)
    {
        if (KHR_D3D11_enable.enabled())
        {
            switch (format)
            {
            case 0x8C43: // GL_SRGB8_ALPHA8
                return 0x1d; // DXGI_FORMAT_R8G8B8A8_UNORM_SRGB
            case 0x8058: // GL_RGBA8
                return 0x1c; // DXGI_FORMAT_R8G8B8A8_UNORM
            case 0x8F97: // GL_RGBA8_SNORM
                return 0x1f; // DXGI_FORMAT_R8G8B8A8_UNORM_SRGB 
            case 0x881A: // GL_RGBA16F
                return 0xa; // DXGI_FORMAT_R8G8B8A8_UNORM_SRGB 
            case 0x8CAC: // GL_DEPTH_COMPONENT32F
            case 0x8DAB: // GL_DEPTH_COMPONENT32F_NV
                return 0x28; // DXGI_FORMAT_D32_FLOAT
            default:
                return 0;
            }
        }

        return 0;
    }

    bool Platform::runtimeSupportsFormat(int64_t format) const
    {
        if (KHR_D3D11_enable.enabled())
        {
            format = openGLFormatToDirectXFormat(format);
            if (format == 0)
                return false;
        }

        auto it =
            std::find(mSwapchainFormats.begin(), mSwapchainFormats.end(), format);
        if (it == std::end(mSwapchainFormats))
        {
            return false;
        }
        return *it;
    }

    int64_t Platform::selectFormat(int64_t preferredFormat, const std::vector<int64_t>& requestedFormats)
    {
        if (KHR_D3D11_enable.enabled())
        {
            preferredFormat = openGLFormatToDirectXFormat(preferredFormat);
        }

        if (preferredFormat != 0)
        {
            auto it =
                std::find(mSwapchainFormats.begin(), mSwapchainFormats.end(), preferredFormat);
            if (it != std::end(mSwapchainFormats))
            {
                return *it;
            }
        }

        auto it =
            std::find_first_of(std::begin(requestedFormats), std::end(requestedFormats),
                mSwapchainFormats.begin(), mSwapchainFormats.end());
        if (it == std::end(requestedFormats))
        {
            return 0;
        }
        return *it;
    }


    std::vector<uint64_t>
        enumerateSwapchainImagesOpenGL(XrSwapchain swapchain)
    {
        XrSwapchainImageOpenGLKHR xrimage{};
        xrimage.type = XR_TYPE_SWAPCHAIN_IMAGE_OPENGL_KHR;
    
        uint32_t imageCount = 0;
        std::vector<uint64_t> images;
        std::vector<XrSwapchainImageOpenGLKHR> xrimages;
        CHECK_XRCMD(xrEnumerateSwapchainImages(swapchain, 0, &imageCount, nullptr));
        xrimages.resize(imageCount, xrimage);
        CHECK_XRCMD(xrEnumerateSwapchainImages(swapchain, imageCount, &imageCount, reinterpret_cast<XrSwapchainImageBaseHeader*>(xrimages.data())));

        for (auto& image : xrimages)
        {
            images.push_back(image.image);
        }

        return images;
    }

#ifdef _WIN32
    std::vector<uint64_t>
        enumerateSwapchainImagesDirectX(XrSwapchain swapchain)
    {
        XrSwapchainImageD3D11KHR xrimage{};
        xrimage.type = XR_TYPE_SWAPCHAIN_IMAGE_D3D11_KHR;
        
        uint32_t imageCount = 0;
        std::vector<uint64_t> images;
        std::vector<XrSwapchainImageD3D11KHR> xrimages;
        CHECK_XRCMD(xrEnumerateSwapchainImages(swapchain, 0, &imageCount, nullptr));
        xrimages.resize(imageCount, xrimage);
        CHECK_XRCMD(xrEnumerateSwapchainImages(swapchain, imageCount, &imageCount, reinterpret_cast<XrSwapchainImageBaseHeader*>(xrimages.data())));

        for (auto& image : xrimages)
        {
            images.push_back(reinterpret_cast<uint64_t>(image.texture));
        }

        return images;
    }
#endif

    VR::Swapchain* Platform::createSwapchain(uint32_t width, uint32_t height, uint32_t samples, VR::SwapchainUse use, const std::string& name, int64_t preferredFormat)
    {
        std::string typeString = use == VR::SwapchainUse::Color ? "color" : "depth";

        XrSwapchainCreateInfo swapchainCreateInfo{};
        swapchainCreateInfo.type = XR_TYPE_SWAPCHAIN_CREATE_INFO;
        swapchainCreateInfo.arraySize = 1;
        swapchainCreateInfo.width = width;
        swapchainCreateInfo.height = height;
        swapchainCreateInfo.mipCount = 1;
        swapchainCreateInfo.faceCount = 1;
        if (use == VR::SwapchainUse::Color)
            swapchainCreateInfo.usageFlags = XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT;
        else
            swapchainCreateInfo.usageFlags = XR_SWAPCHAIN_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;

        XrSwapchain swapchain = XR_NULL_HANDLE;
        int format = 0;

        while (samples > 0 && swapchain == XR_NULL_HANDLE && format == 0)
        {
            // Select a swapchain format.
            if (use == VR::SwapchainUse::Color)
                format = selectColorFormat(preferredFormat);
            else
                format = selectDepthFormat(preferredFormat);
            if (format == 0) {
                throw std::runtime_error(std::string("Swapchain ") + typeString + " format not supported");
            }
            Log(Debug::Verbose) << "Selected " << typeString << " format: " << std::dec << format << " (" << std::hex << format << ")" << std::dec;

            // Now create the swapchain
            Log(Debug::Verbose) << "Creating swapchain with dimensions Width=" << width << " Heigh=" << height << " SampleCount=" << samples;
            swapchainCreateInfo.format = format;
            swapchainCreateInfo.sampleCount = samples;
            auto res = xrCreateSwapchain(Session::instance().xrSession(), &swapchainCreateInfo, &swapchain);

            // Check errors and try again if possible
            if (res == XR_ERROR_SWAPCHAIN_FORMAT_UNSUPPORTED)
            {
                // We only try swapchain formats enumerated by the runtime itself.
                // This does not guarantee that that swapchain format is going to be supported for this specific usage.
                Log(Debug::Verbose) << "Failed to create swapchain with Format=" << format << ": " << XrResultString(res);
                eraseFormat(format);
                format = 0;
                continue;
            }
            else if (!XR_SUCCEEDED(res))
            {
                Log(Debug::Verbose) << "Failed to create swapchain with SampleCount=" << samples << ": " << XrResultString(res);
                samples /= 2;
                if (samples == 0)
                {
                    CHECK_XRRESULT(res, "xrCreateSwapchain");
                    throw std::runtime_error(XrResultString(res));
                }
                continue;
            }

            CHECK_XRRESULT(res, "xrCreateSwapchain");
            if (use == VR::SwapchainUse::Color)
                Debugging::setName(swapchain, "OpenMW XR Color Swapchain " + name);
            else
                Debugging::setName(swapchain, "OpenMW XR Depth Swapchain " + name);

            if (KHR_opengl_enable.enabled())
            {
                auto images = enumerateSwapchainImagesOpenGL(swapchain);
                return new Swapchain(swapchain, images, width, height, samples, format);
            }
#ifdef _WIN32
            else if (KHR_D3D11_enable.enabled())
            {
                auto images = enumerateSwapchainImagesDirectX(swapchain);
                return new VR::DirectXSwapchain(std::make_shared<Swapchain>(swapchain, images, width, height, samples, format), mDxInterop);
            }
#endif
            else
            {
                // TODO: Vulkan swapchains?
                throw std::logic_error("Not implemented");
            }
        }

        // Never actually reached
        return nullptr;
    }
}
