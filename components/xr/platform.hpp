#ifndef XR_PLATFORM_HPP
#define XR_PLATFORM_HPP

#include <vector>
#include <memory>
#include <set>
#include <string>

#include <openxr/openxr.h>

#include <components/vr/constants.hpp>
#include <components/vr/directx.hpp>

namespace XR
{
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

        void selectGraphicsAPIExtension();
        XrSession createXrSession(XrInstance instance, XrSystemId systemId);

        int64_t selectColorFormat();
        int64_t selectDepthFormat();
        int64_t selectFormat(const std::vector<int64_t>& requestedFormats);
        void eraseFormat(int64_t format);
        std::vector<int64_t> mSwapchainFormats{};

        VR::Swapchain* createSwapchain(uint32_t width, uint32_t height, uint32_t samples, VR::SwapchainUse use, const std::string& name);

    private:
        bool selectDirectX();
        bool selectOpenGL();


        std::unique_ptr< PlatformPrivate > mPrivate;
        std::shared_ptr<VR::DirectXWGLInterop> mDxInterop = nullptr;
    };
}

#endif
