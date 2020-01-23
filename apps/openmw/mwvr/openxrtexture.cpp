#include "openxrviewer.hpp"
#include "openxrtexture.hpp"
#include <osg/Texture2D>
#include <osgViewer/Renderer>
#include <components/debug/debuglog.hpp>
#include <osgDB/Registry>
#include <sstream>
#include <fstream>

#ifndef GL_TEXTURE_MAX_LEVEL
#define GL_TEXTURE_MAX_LEVEL 0x813D
#endif

namespace MWVR
{
    OpenXRTextureBuffer::OpenXRTextureBuffer(
        osg::ref_ptr<osg::State> state,
        uint32_t XRColorBuffer, 
        std::size_t width, 
        std::size_t height,
        uint32_t msaaSamples)
        : mState(state)
        , mWidth(width)
        , mHeight(height)
        , mXRColorBuffer(XRColorBuffer)
        , mMSAASamples(msaaSamples)
    {

        auto* gl = osg::GLExtensions::Get(state->getContextID(), false);
        glBindTexture(GL_TEXTURE_2D, XRColorBuffer);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

        GLint w = 0;
        GLint h = 0;
        glGetTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_WIDTH, &w);
        glGetTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_HEIGHT, &h);

        gl->glGenFramebuffers(1, &mFBO);

        if (mMSAASamples == 0)
        {
            glGenTextures(1, &mDepthBuffer);
            glBindTexture(GL_TEXTURE_2D, mDepthBuffer);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT24, mWidth, mHeight, 0, GL_DEPTH_COMPONENT, GL_UNSIGNED_INT, nullptr);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0);

            gl->glBindFramebuffer(GL_FRAMEBUFFER_EXT, mFBO);
            gl->glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT0_EXT, GL_TEXTURE_2D, mXRColorBuffer, 0);
            gl->glFramebufferTexture2D(GL_FRAMEBUFFER_EXT, GL_DEPTH_ATTACHMENT_EXT, GL_TEXTURE_2D, mDepthBuffer, 0);

            if (gl->glCheckFramebufferStatus(GL_FRAMEBUFFER_EXT) != GL_FRAMEBUFFER_COMPLETE_EXT)
                throw std::runtime_error("Failed to create OpenXR framebuffer");
        }
        else
        {
            gl->glGenFramebuffers(1, &mMSAAFBO);

            // Create MSAA color buffer
            glGenTextures(1, &mMSAAColorBuffer);
            glBindTexture(GL_TEXTURE_2D_MULTISAMPLE, mMSAAColorBuffer);
            gl->glTexImage2DMultisample(GL_TEXTURE_2D_MULTISAMPLE, mMSAASamples, GL_RGBA, mWidth, mHeight, false);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER_ARB);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER_ARB);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D_MULTISAMPLE, GL_TEXTURE_MAX_LEVEL, 0);

            // Create MSAA depth buffer
            glGenTextures(1, &mMSAADepthBuffer);
            glBindTexture(GL_TEXTURE_2D_MULTISAMPLE, mMSAADepthBuffer);
            gl->glTexImage2DMultisample(GL_TEXTURE_2D_MULTISAMPLE, mMSAASamples, GL_DEPTH_COMPONENT, mWidth, mHeight, false);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D_MULTISAMPLE, GL_TEXTURE_MAX_LEVEL, 0);

            gl->glBindFramebuffer(GL_FRAMEBUFFER_EXT, mMSAAFBO);
            gl->glFramebufferTexture2D(GL_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT0_EXT, GL_TEXTURE_2D_MULTISAMPLE, mMSAAColorBuffer, 0);
            gl->glFramebufferTexture2D(GL_FRAMEBUFFER_EXT, GL_DEPTH_ATTACHMENT_EXT, GL_TEXTURE_2D_MULTISAMPLE, mMSAADepthBuffer, 0);
            if (gl->glCheckFramebufferStatus(GL_FRAMEBUFFER_EXT) != GL_FRAMEBUFFER_COMPLETE_EXT)
                throw std::runtime_error("Failed to create MSAA framebuffer");

            gl->glBindFramebuffer(GL_FRAMEBUFFER_EXT, mFBO);
            gl->glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT0_EXT, GL_TEXTURE_2D, mXRColorBuffer, 0);
            gl->glFramebufferRenderbuffer(GL_DRAW_FRAMEBUFFER_EXT, GL_DEPTH_ATTACHMENT_EXT, GL_RENDERBUFFER_EXT, 0);
            if (gl->glCheckFramebufferStatus(GL_FRAMEBUFFER_EXT) != GL_FRAMEBUFFER_COMPLETE_EXT)
                throw std::runtime_error("Failed to create OpenXR framebuffer");
        }


        gl->glBindFramebuffer(GL_FRAMEBUFFER_EXT, 0);
    }

    OpenXRTextureBuffer::~OpenXRTextureBuffer()
    {
        destroy(nullptr);
    }

    void OpenXRTextureBuffer::destroy(osg::State* state)
    {
        if (!state)
        {
            // Try re-using the state received during construction
            state = mState.get();
        }

        if (state)
        {
            auto* gl = osg::GLExtensions::Get(state->getContextID(), false);
            if (mFBO)
                gl->glDeleteFramebuffers(1, &mFBO);
            if (mMSAAFBO)
                gl->glDeleteFramebuffers(1, &mMSAAFBO);
        }
        else if(mFBO || mMSAAFBO)
            // Without access to opengl methods, i'll just let the FBOs leak.
            Log(Debug::Warning) << "destroy() called without a State. Leaking FBOs";
            
        if (mDepthBuffer)
            glDeleteTextures(1, &mDepthBuffer);
        if (mMSAAColorBuffer)
            glDeleteTextures(1, &mMSAAColorBuffer);
        if (mMSAADepthBuffer)
            glDeleteTextures(1, &mMSAADepthBuffer);

        mFBO = mMSAAFBO = mDepthBuffer = mMSAAColorBuffer = mMSAADepthBuffer = 0;
    }

    void OpenXRTextureBuffer::beginFrame(osg::GraphicsContext* gc)
    {
        auto state = gc->getState();
        auto* gl = osg::GLExtensions::Get(state->getContextID(), false);


        if (mMSAASamples == 0)
        {
            gl->glBindFramebuffer(GL_FRAMEBUFFER_EXT, mFBO);
        }
        else
        {
            gl->glBindFramebuffer(GL_FRAMEBUFFER_EXT, mMSAAFBO);
        }
    }

    void OpenXRTextureBuffer::endFrame(osg::GraphicsContext* gc)
    {
        auto* state = gc->getState();
        auto* gl = osg::GLExtensions::Get(state->getContextID(), false);
        if (mMSAASamples == 0)
        {
            gl->glBindFramebuffer(GL_FRAMEBUFFER_EXT, 0);
        }
        else
        {
            gl->glBindFramebuffer(GL_READ_FRAMEBUFFER_EXT, mMSAAFBO);
            gl->glBindFramebuffer(GL_DRAW_FRAMEBUFFER_EXT, mFBO);
            gl->glBlitFramebuffer(0, 0, mWidth, mHeight, 0, 0, mWidth, mHeight, GL_COLOR_BUFFER_BIT, GL_NEAREST);
            gl->glBindFramebuffer(GL_DRAW_FRAMEBUFFER_EXT, 0);
            gl->glBindFramebuffer(GL_READ_FRAMEBUFFER_EXT, 0);
        }
    }

    void OpenXRTextureBuffer::blit(osg::GraphicsContext* gc, int x, int y, int w, int h)
    {
        auto* state = gc->getState();
        auto* gl = osg::GLExtensions::Get(state->getContextID(), false);
        gl->glBindFramebuffer(GL_READ_FRAMEBUFFER_EXT, mFBO);
        gl->glBlitFramebuffer(0, 0, mWidth, mHeight, x, y, w, h, GL_COLOR_BUFFER_BIT, GL_NEAREST);
        gl->glBindFramebuffer(GL_READ_FRAMEBUFFER_EXT, 0);
    }
}
