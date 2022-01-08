#ifndef XR_PLATFORM_HPP
#define XR_PLATFORM_HPP

#include <vector>
#include <memory>
#include <set>
#include <string>

#include <openxr/openxr.h>

#include <components/vr/constants.hpp>
#include <components/vr/directx.hpp>
#include <components/vr/swapchain.hpp>

#include <osg/GraphicsContext>

namespace XR
{
    struct PlatformPrivate;

    class Platform
    {
    public:
        Platform(osg::GraphicsContext* gc);
        ~Platform();

        void selectGraphicsAPIExtension();
        XrSession createXrSession(XrInstance instance, XrSystemId systemId);

        int64_t selectColorFormat(int64_t preferredFormat);
        int64_t selectDepthFormat(int64_t preferredFormat);
        int64_t selectFormat(int64_t preferredFormat, const std::vector<int64_t>& requestedFormats);
        
        std::vector<GLenum> supportedSwapchainFormatsGL() const;
        std::vector<int64_t> supportedSwapchainFormatsDX() const;

        VR::Swapchain* createSwapchain(uint32_t width, uint32_t height, uint32_t samples, uint32_t arraySize, VR::SwapchainUse use, const std::string& name, int64_t preferredFormat = 0);

    private:
        bool selectDirectX();
        bool selectOpenGL();

        std::unique_ptr< PlatformPrivate > mPrivate;
        std::shared_ptr<VR::DirectXWGLInterop> mDxInterop = nullptr;
        std::vector<int64_t> mSwapchainFormatsDX;
        std::vector<GLenum> mSwapchainFormatsGL;
    };
}

#endif
