#ifndef XR_PLATFORM_HPP
#define XR_PLATFORM_HPP

#include <vector>
#include <memory>
#include <set>
#include <string>

#include <openxr/openxr.h>

#include <components/vr/directx.hpp>

namespace XR
{
    // Error management macros and functions. Should be used on every openxr call.
#define CHK_STRINGIFY(x) #x
#define TOSTRING(x) CHK_STRINGIFY(x)
#define FILE_AND_LINE __FILE__ ":" TOSTRING(__LINE__)
#define CHECK_XRCMD(cmd) ::XR::CheckXrResult(cmd, #cmd, FILE_AND_LINE)
#define CHECK_XRRESULT(res, cmdStr) ::XR::CheckXrResult(res, cmdStr, FILE_AND_LINE)
    XrResult CheckXrResult(XrResult res, const char* originator = nullptr, const char* sourceLocation = nullptr);
    std::string XrResultString(XrResult res);

    struct PlatformPrivate;

    class Platform
    {
    public:
        using ExtensionMap = std::map<std::string, XrExtensionProperties>;
        using LayerMap = std::map<std::string, XrApiLayerProperties>;
        using LayerExtensionMap = std::map<std::string, ExtensionMap>;

    public:
        Platform(osg::GraphicsContext* gc);
        ~Platform();

        const char* graphicsAPIExtensionName();
        bool supportsExtension(const std::string& extensionName) const;
        bool supportsExtension(const std::string& extensionName, uint32_t minimumVersion) const;
        bool supportsLayer(const std::string& layerName) const;
        bool supportsLayer(const std::string& layerName, uint32_t minimumVersion) const;

        bool enableExtension(const std::string& extensionName, bool optional);
        bool enableExtension(const std::string& extensionName, bool optional, uint32_t minimumVersion);

        bool extensionEnabled(const std::string& extensionName) const;

        XrInstance createXrInstance(const std::string& name);
        XrSession createXrSession(XrInstance instance, XrSystemId systemId);

        int64_t selectColorFormat();
        int64_t selectDepthFormat();
        int64_t selectFormat(const std::vector<int64_t>& requestedFormats);
        void eraseFormat(int64_t format);
        std::vector<int64_t> mSwapchainFormats{};

        ///// Registers an object for sharing as if calling wglDXRegisterObjectNV requesting write access.
        ///// If ntShareHandle is not null, wglDXSetResourceShareHandleNV is called first to register the share handle
        //void* DXRegisterObject(void* dxResource, uint32_t glName, uint32_t glType, bool discard, void* ntShareHandle);
        ///// Unregisters an object from sharing as if calling wglDXUnregisterObjectNV
        //void  DXUnregisterObject(void* dxResourceShareHandle);
        ///// Locks a DX object for use by OpenGL as if calling wglDXLockObjectsNV
        //void  DXLockObject(void* dxResourceShareHandle);
        ///// Unlocks a DX object for use by DirectX as if calling wglDXUnlockObjectsNV
        //void  DXUnlockObject(void* dxResourceShareHandle);

        std::string getInstanceName(XrInstance instance);
        XrVersion getInstanceVersion(XrInstance instance);
        void initFailure(XrResult, XrInstance);

        std::shared_ptr<VR::DirectXWGLInterop> dxInterop();

        enum class SwapchainUse
        {
            Color,
            Depth,
        };
        VR::Swapchain* createSwapchain(uint32_t width, uint32_t height, uint32_t samples, SwapchainUse use, const std::string& name);

    private:
        void enumerateExtensions(const char* layerName, int logIndent);
        void setupExtensions();
        void selectGraphicsAPIExtension();
        bool selectDirectX();
        bool selectOpenGL();

        ExtensionMap mAvailableExtensions;
        LayerMap mAvailableLayers;
        LayerExtensionMap mAvailableLayerExtensions;
        std::vector<const char*> mEnabledExtensions;
        const char* mGraphicsAPIExtension = nullptr;

        std::unique_ptr< PlatformPrivate > mPrivate;
        std::shared_ptr<VR::DirectXWGLInterop> mDxInterop = nullptr;
    };
}

#endif
