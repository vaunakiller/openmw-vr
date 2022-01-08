#include "depth.hpp"

#include <algorithm>

#include <SDL_opengl_glext.h>

#include <components/debug/debuglog.hpp>
#include <components/settings/settings.hpp>

#ifndef GL_DEPTH32F_STENCIL8_NV
#define GL_DEPTH32F_STENCIL8_NV 0x8DAC
#endif

namespace SceneUtil
{
    void setCameraClearDepth(osg::Camera* camera)
    {
        camera->setClearDepth(AutoDepth::isReversed() ? 0.0 : 1.0);
    }

    osg::Matrix getReversedZProjectionMatrixAsPerspectiveInf(double fov, double aspect, double near)
    {
        double A = 1.0/std::tan(osg::DegreesToRadians(fov)/2.0);
        return osg::Matrix(
            A/aspect,   0,      0,      0,
            0,          A,      0,      0,
            0,          0,      0,      -1,
            0,          0,      near,   0
        );
    }

    osg::Matrix getReversedZProjectionMatrixAsPerspective(double fov, double aspect, double near, double far)
    {
        double A = 1.0/std::tan(osg::DegreesToRadians(fov)/2.0);
        return osg::Matrix(
            A/aspect,   0,      0,                          0,
            0,          A,      0,                          0,
            0,          0,      near/(far-near),            -1,
            0,          0,      (far*near)/(far - near),    0
        );
    }

    osg::Matrix getReversedZProjectionMatrixAsOrtho(double left, double right, double bottom, double top, double near, double far)
    {
        return osg::Matrix(
            2/(right-left),             0,                          0,                  0,
            0,                          2/(top-bottom),             0,                  0,
            0,                          0,                          1/(far-near),       0,
            (right+left)/(left-right),  (top+bottom)/(bottom-top),  far/(far-near),     1
        );
    }

    bool isFloatingPointDepthFormat(GLenum format)
    {
        constexpr std::array<GLenum, 4> formats = {
            GL_DEPTH_COMPONENT32F,
            GL_DEPTH_COMPONENT32F_NV,
            GL_DEPTH32F_STENCIL8,
            GL_DEPTH32F_STENCIL8_NV,
        };

        return std::find(formats.cbegin(), formats.cend(), format) != formats.cend();
    }

    void SelectDepthFormatOperation::operator()(osg::GraphicsContext* graphicsContext)
    {
        bool enableReverseZ = false;

        if (Settings::Manager::getBool("reverse z", "Camera"))
        {
            osg::ref_ptr<osg::GLExtensions> exts = osg::GLExtensions::Get(0, false);
            if (exts && exts->isClipControlSupported)
            {
                enableReverseZ = true;
                Log(Debug::Info) << "Using reverse-z depth buffer";
            }
            else
                Log(Debug::Warning) << "GL_ARB_clip_control not supported: disabling reverse-z depth buffer";
        }
        else
            Log(Debug::Info) << "Using standard depth buffer";

        SceneUtil::AutoDepth::setReversed(enableReverseZ);

        constexpr char errPreamble[] = "Postprocessing and floating point depth buffers disabled: ";
        std::vector<GLenum> requestedFormats;
        unsigned int contextID = graphicsContext->getState()->getContextID();
        osg::GLExtensions* ext = graphicsContext->getState()->get<osg::GLExtensions>();
        if (SceneUtil::AutoDepth::isReversed())
        {
            if (osg::isGLExtensionSupported(contextID, "GL_ARB_depth_buffer_float"))
                requestedFormats.push_back(GL_DEPTH_COMPONENT32F);
            else if (osg::isGLExtensionSupported(contextID, "GL_NV_depth_buffer_float"))
                requestedFormats.push_back(GL_DEPTH_COMPONENT32F_NV);
            else
            {
                Log(Debug::Warning) << errPreamble << "'GL_ARB_depth_buffer_float' and 'GL_NV_depth_buffer_float' unsupported.";
            }
        }
        requestedFormats.push_back(GL_DEPTH_COMPONENT24);

        if (mSupportedFormats.empty())
        {
            SceneUtil::AutoDepth::setDepthFormat(requestedFormats.front());
        }
        else
        {
            requestedFormats.push_back(GL_DEPTH24_STENCIL8);
            requestedFormats.push_back(GL_DEPTH_COMPONENT32);
            requestedFormats.push_back(0x81A6); // GL_DEPTH_COMPONENT24
            for (auto requestedFormat : requestedFormats)
            {
                if (std::find(mSupportedFormats.cbegin(), mSupportedFormats.cend(), requestedFormat) != mSupportedFormats.cend())
                {
                    SceneUtil::AutoDepth::setDepthFormat(requestedFormat);
                    break;
                }
            }
        }
    }
}
