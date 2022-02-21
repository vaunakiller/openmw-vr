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
        
        const std::vector<GLenum>& supportedColorFormats() const { return mMWColorFormatsGL; };
        const std::vector<GLenum>& supportedDepthFormats() const { return mMWDepthFormatsGL; };

        VR::Swapchain* createSwapchain(uint32_t width, uint32_t height, uint32_t samples, uint32_t arraySize, VR::SwapchainUse use, const std::string& name);

    private:
        bool selectDirectX();
        bool selectOpenGL();

        std::unique_ptr< PlatformPrivate > mPrivate;
        std::shared_ptr<VR::DirectXWGLInterop> mDxInterop = nullptr;
        std::vector<int64_t> mRuntimeFormatsDX;
        std::vector<GLenum> mRuntimeFormatsGL;
        std::vector<GLenum> mMWColorFormatsGL;
        std::vector<GLenum> mMWDepthFormatsGL;
    };
}

#endif
