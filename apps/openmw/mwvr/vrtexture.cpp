#include "vrtexture.hpp"

#include <osg/Texture2D>

#include <components/debug/debuglog.hpp>

#ifndef GL_TEXTURE_MAX_LEVEL
#define GL_TEXTURE_MAX_LEVEL 0x813D
#endif

namespace MWVR
{

    VRTexture::VRTexture(osg::ref_ptr<osg::State> state, std::size_t width, std::size_t height, uint32_t msaaSamples, uint32_t colorBuffer, uint32_t depthBuffer)
        : mState(state)
        , mWidth(width)
        , mHeight(height)
        , mDepthBuffer(depthBuffer)
        , mColorBuffer(colorBuffer)
        , mSamples(msaaSamples)
    {
        auto* gl = osg::GLExtensions::Get(state->getContextID(), false);

        gl->glGenFramebuffers(1, &mBlitFBO);
        gl->glBindFramebuffer(GL_FRAMEBUFFER_EXT, mBlitFBO);

        gl->glGenFramebuffers(1, &mFBO);

        if (mSamples <= 1)
            mTextureTarget = GL_TEXTURE_2D;
        else
            mTextureTarget = GL_TEXTURE_2D_MULTISAMPLE;

        if (mColorBuffer == 0)
        {
            glGenTextures(1, &mColorBuffer);
            glBindTexture(mTextureTarget, mColorBuffer);
            if (mSamples <= 1)
                glTexImage2D(mTextureTarget, 0, GL_RGBA, mWidth, mHeight, 0, GL_RGBA, GL_UNSIGNED_INT, nullptr);
            else
                gl->glTexImage2DMultisample(mTextureTarget, mSamples, GL_RGBA, mWidth, mHeight, false);
            glTexParameteri(mTextureTarget, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER_ARB);
            glTexParameteri(mTextureTarget, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER_ARB);
            glTexParameteri(mTextureTarget, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
            glTexParameteri(mTextureTarget, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glTexParameteri(mTextureTarget, GL_TEXTURE_MAX_LEVEL, 0);
        }

        if (mDepthBuffer == 0)
        {
            glGenTextures(1, &mDepthBuffer);
            glBindTexture(mTextureTarget, mDepthBuffer);
            if (mSamples <= 1)
                glTexImage2D(mTextureTarget, 0, GL_DEPTH_COMPONENT24, mWidth, mHeight, 0, GL_DEPTH_COMPONENT, GL_UNSIGNED_INT, nullptr);
            else
                gl->glTexImage2DMultisample(mTextureTarget, mSamples, GL_DEPTH_COMPONENT, mWidth, mHeight, false);
            glTexParameteri(mTextureTarget, GL_TEXTURE_WRAP_S, GL_REPEAT);
            glTexParameteri(mTextureTarget, GL_TEXTURE_WRAP_T, GL_REPEAT);
            glTexParameteri(mTextureTarget, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
            glTexParameteri(mTextureTarget, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glTexParameteri(mTextureTarget, GL_TEXTURE_MAX_LEVEL, 0);
        }


        gl->glBindFramebuffer(GL_FRAMEBUFFER_EXT, mFBO);
        gl->glFramebufferTexture2D(GL_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT0_EXT, mTextureTarget, mColorBuffer, 0);
        gl->glFramebufferTexture2D(GL_FRAMEBUFFER_EXT, GL_DEPTH_ATTACHMENT_EXT, mTextureTarget, mDepthBuffer, 0);
        if (gl->glCheckFramebufferStatus(GL_FRAMEBUFFER_EXT) != GL_FRAMEBUFFER_COMPLETE_EXT)
            throw std::runtime_error("Failed to create OpenXR framebuffer");


        gl->glBindFramebuffer(GL_FRAMEBUFFER_EXT, 0);
    }

    VRTexture::~VRTexture()
    {
        destroy(nullptr);
    }

    void VRTexture::destroy(osg::State* state)
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
        }
        else if (mFBO)
            // Without access to opengl methods, i'll just let the FBOs leak.
            Log(Debug::Warning) << "destroy() called without a State. Leaking FBO";

        if (mDepthBuffer)
            glDeleteTextures(1, &mDepthBuffer);
        if (mColorBuffer)
            glDeleteTextures(1, &mColorBuffer);

        mFBO = mDepthBuffer = mColorBuffer;
    }

    void VRTexture::beginFrame(osg::GraphicsContext* gc)
    {
        auto state = gc->getState();
        auto* gl = osg::GLExtensions::Get(state->getContextID(), false);
        gl->glBindFramebuffer(GL_FRAMEBUFFER_EXT, mFBO);
    }

    void VRTexture::endFrame(osg::GraphicsContext* gc, uint32_t blitTarget)
    {
        auto* state = gc->getState();
        auto* gl = osg::GLExtensions::Get(state->getContextID(), false);
        gl->glBindFramebuffer(GL_DRAW_FRAMEBUFFER_EXT, mBlitFBO);
        gl->glBindFramebuffer(GL_READ_FRAMEBUFFER_EXT, mFBO);
        gl->glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT0_EXT, GL_TEXTURE_2D, blitTarget, 0);
        gl->glBlitFramebuffer(0, 0, mWidth, mHeight, 0, 0, mWidth, mHeight, GL_COLOR_BUFFER_BIT, GL_NEAREST);
        gl->glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT0_EXT, GL_TEXTURE_2D, 0, 0);
        gl->glBindFramebuffer(GL_DRAW_FRAMEBUFFER_EXT, 0);
        gl->glBindFramebuffer(GL_READ_FRAMEBUFFER_EXT, 0);
    }

    void VRTexture::blit(osg::GraphicsContext* gc, int x, int y, int w, int h)
    {
        auto* state = gc->getState();
        auto* gl = osg::GLExtensions::Get(state->getContextID(), false);
        gl->glBindFramebuffer(GL_READ_FRAMEBUFFER_EXT, mFBO);
        gl->glBlitFramebuffer(0, 0, mWidth, mHeight, x, y, w, h, GL_COLOR_BUFFER_BIT, GL_LINEAR);
        gl->glBindFramebuffer(GL_READ_FRAMEBUFFER_EXT, 0);
    }

    void VRTexture::blit(osg::GraphicsContext* gc, int x, int y, int w, int h, int blitTarget)
    {
        auto* state = gc->getState();
        auto* gl = osg::GLExtensions::Get(state->getContextID(), false);
        gl->glBindFramebuffer(GL_DRAW_FRAMEBUFFER_EXT, mBlitFBO);
        gl->glBindFramebuffer(GL_READ_FRAMEBUFFER_EXT, mFBO);
        gl->glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT0_EXT, GL_TEXTURE_2D, blitTarget, 0);
        gl->glBlitFramebuffer(0, 0, mWidth, mHeight, x, y, w, h, GL_COLOR_BUFFER_BIT, GL_LINEAR);
        gl->glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT0_EXT, GL_TEXTURE_2D, 0, 0);
        gl->glBindFramebuffer(GL_DRAW_FRAMEBUFFER_EXT, 0);
        gl->glBindFramebuffer(GL_READ_FRAMEBUFFER_EXT, 0);
    }
}
