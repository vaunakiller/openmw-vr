#include "color.hpp"

#include <algorithm>

#include <SDL_opengl_glext.h>

#include <components/debug/debuglog.hpp>
#include <components/settings/settings.hpp>

namespace SceneUtil
{
    namespace ColorFormat
    {
        GLenum sColorFormat;

        GLenum colorFormat()
        {
            return sColorFormat;
        }

        bool isColorFormat(GLenum format)
        {
            switch (format)
            {
            case GL_RGB:
            case GL_RGB4:
            case GL_RGB5:
            case GL_RGB8:
            case GL_RGB8_SNORM:
            case GL_RGB10:
            case GL_RGB12:
            case GL_RGB16:
            case GL_RGB16_SNORM:
            case GL_SRGB:
            case GL_SRGB8:
            case GL_RGB16F:
            case GL_RGB32F:
            case GL_R11F_G11F_B10F:
            case GL_RGB9_E5:
            case GL_RGB8I:
            case GL_RGB8UI:
            case GL_RGB16I:
            case GL_RGB16UI:
            case GL_RGB32I:
            case GL_RGB32UI:
            case GL_RGBA:
            case GL_RGBA2:
            case GL_RGBA4:
            case GL_RGB5_A1:
            case GL_RGBA8:
            case GL_RGBA8_SNORM:
            case GL_RGB10_A2:
            case GL_RGB10_A2UI:
            case GL_RGBA12:
            case GL_RGBA16:
            case GL_RGBA16_SNORM:
            case GL_SRGB_ALPHA8:
            case GL_SRGB8_ALPHA8:
            case GL_RGBA16F:
            case GL_RGBA32F:
            case GL_RGBA8I:
            case GL_RGBA8UI:
            case GL_RGBA16I:
            case GL_RGBA16UI:
            case GL_RGBA32I:
            case GL_RGBA32UI:
                return true;

            default:
                return false;
            }
        }

        void SelectColorFormatOperation::operator()(osg::GraphicsContext* graphicsContext)
        {
            (void)graphicsContext;
            sColorFormat = GL_RGB;

            for (auto supportedFormat : mSupportedFormats)
            {
                if (isColorFormat(supportedFormat))
                {
                    sColorFormat = supportedFormat;
                    break;
                }
            }
        }
    }
}
