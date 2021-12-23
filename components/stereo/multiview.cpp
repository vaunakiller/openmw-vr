#include "multiview.hpp"

#include <osg/FrameBufferObject>
#include <osg/GLExtensions>
#include <osg/Texture2D>
#include <osg/Texture2DMultisample>
#include <osg/Texture2DArray>
#include <osg/Texture2DMultisampleArray>

#include <components/settings/settings.hpp>
#include <components/debug/debuglog.hpp>

#include <algorithm>

namespace Stereo
{
    namespace
    {
        bool getMultiviewSupportedImpl(unsigned int contextID)
        {
#ifdef OSG_HAS_MULTIVIEW
            if (!osg::isGLExtensionSupported(contextID, "GL_OVR_multiview"))
            {
                Log(Debug::Verbose) << "Disabling Multiview (opengl extension \"GL_OVR_multiview\" not supported)";
                return false;
            }

            if (!osg::isGLExtensionSupported(contextID, "GL_OVR_multiview2"))
            {
                Log(Debug::Verbose) << "Disabling Multiview (opengl extension \"GL_OVR_multiview2\" not supported)";
                return false;
            }
            return true;
#else
            Log(Debug::Verbose) << "Disabling Multiview (OSG does not support multiview)";
            return false;
#endif
        }

        bool getMultiviewSupported(unsigned int contextID)
        {
            static bool supported = getMultiviewSupportedImpl(contextID);
            return supported;
        }

        bool getTextureViewSupportedImpl(unsigned int contextID)
        {
            if (!osg::isGLExtensionOrVersionSupported(contextID, "ARB_texture_view", 4.3))
            {
                Log(Debug::Verbose) << "Disabling texture views (opengl extension \"ARB_texture_view\" not supported)";
                return false;
            }
            return true;
        }

        bool getTextureViewSupported(unsigned int contextID)
        {
            static bool supported = getTextureViewSupportedImpl(contextID);
            return supported;
        }

        bool getMultiviewImpl(unsigned int contextID)
        {
            if (!Settings::Manager::getBool("stereo enabled", "Stereo"))
            {
                Log(Debug::Verbose) << "Disabling Multiview (disabled by config)";
                return false;
            }

            if (!Settings::Manager::getBool("multiview", "Stereo"))
            {
                Log(Debug::Verbose) << "Disabling Multiview (disabled by config)";
                return false;
            }

            if (!getMultiviewSupported(contextID))
            {
                return false;
            }

            if (!getTextureViewSupported(contextID))
            {
                Log(Debug::Verbose) << "Disabling Multiview (texture views not supported)";
                return false;
            }

            Log(Debug::Verbose) << "Enabling Multiview";
            return true;
        }

        bool getMultiview(unsigned int contextID)
        {
            static bool multiView = getMultiviewImpl(contextID);
            return multiView;
        }
    }

    bool getTextureViewSupported()
    {
        return getTextureViewSupported(0);
    }

    bool getMultiview()
    {
        return getMultiview(0);
    }

    void configureExtensions(unsigned int contextID)
    {
        getTextureViewSupported(contextID);
        getMultiviewSupported(contextID);
        getMultiview(contextID);
    }

    class Texture2DViewSubloadCallback : public osg::Texture2D::SubloadCallback
    {
    public:
        Texture2DViewSubloadCallback(osg::Texture2DArray* textureArray, int layer);
        ~Texture2DViewSubloadCallback() override;

        void load(const osg::Texture2D& texture, osg::State& state) const override;
        void subload(const osg::Texture2D& texture, osg::State& state) const override;

    private:
        osg::ref_ptr<osg::Texture2DArray> mTextureArray;
        int mLayer;
    };

    Texture2DViewSubloadCallback::Texture2DViewSubloadCallback(osg::Texture2DArray* textureArray, int layer)
        : mTextureArray(textureArray)
        , mLayer(layer)
    {
    }

    Texture2DViewSubloadCallback::~Texture2DViewSubloadCallback()
    {
    }

    void Texture2DViewSubloadCallback::load(const osg::Texture2D& texture, osg::State& state) const
    {
        auto contextId = state.getContextID();
        auto* gl = osg::GLExtensions::Get(contextId, false);
        mTextureArray->apply(state);

        auto sourceTextureObject = mTextureArray->getTextureObject(contextId);
        if (!sourceTextureObject)
        {
            Log(Debug::Error) << "Texture2DViewSubloadCallback: Texture2DArray did not have a texture object";
            return;
        }

        auto targetTextureObject = texture.getTextureObject(contextId);
        if (!sourceTextureObject)
        {
            Log(Debug::Error) << "Texture2DViewSubloadCallback: Texture2D did not have a texture object";
            return;
        }

        // OSG already bound this texture ID, giving it a target.
        // Delete it and make a new texture ID.
        glBindTexture(GL_TEXTURE_2D, 0);
        glDeleteTextures(1, &targetTextureObject->_id);
        glGenTextures(1, &targetTextureObject->_id);

        auto sourceId = sourceTextureObject->_id;
        auto targetId = targetTextureObject->_id;
        auto internalFormat = sourceTextureObject->_profile._internalFormat;
        auto levels = std::max(1, sourceTextureObject->_profile._numMipmapLevels);

        {
            ////// OSG BUG
            // Texture views require immutable storage. 
            // OSG should always give immutable storage to sized internal formats, but does not do so for depth formats.
            // Fortunately, we can just call glTexStorage3D here to make it immutable.
#ifndef GL_TEXTURE_IMMUTABLE_FORMAT
#define GL_TEXTURE_IMMUTABLE_FORMAT 0x912F 
#endif
            // Store any current binding and re-apply it after so i don't mess with state.
            GLint oldBinding = 0;
            glGetIntegerv(GL_TEXTURE_BINDING_2D_ARRAY, &oldBinding);

            // Bind the source texture and check if it's immutable.
            glBindTexture(GL_TEXTURE_2D_ARRAY, sourceId);
            GLint immutable = 0;
            glGetTexParameteriv(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_IMMUTABLE_FORMAT, &immutable);
            if(!immutable)
            {
                // It wasn't immutable, so make it immutable.
                gl->glTexStorage3D(GL_TEXTURE_2D_ARRAY, 1, internalFormat, sourceTextureObject->_profile._width, sourceTextureObject->_profile._height, 2);
                state.checkGLErrors("after Texture2DViewSubloadCallback::load()::glTexStorage3D");
            }
            glBindTexture(GL_TEXTURE_2D_ARRAY, oldBinding);
        }

        gl->glTextureView(targetId, GL_TEXTURE_2D, sourceId, internalFormat, 0, levels, mLayer, 1);
        state.checkGLErrors("after Texture2DViewSubloadCallback::load()::glTextureView");
        glBindTexture(GL_TEXTURE_2D, targetId);
    }

    void Texture2DViewSubloadCallback::subload(const osg::Texture2D& texture, osg::State& state) const
    {
        // Nothing to do
    }

    osg::ref_ptr<osg::Texture2D> createTextureView_Texture2DFromTexture2DArray(osg::Texture2DArray* textureArray, int layer)
    {
        if (!getTextureViewSupported())
        {
            Log(Debug::Error) << "createTextureView_Texture2DFromTexture2DArray: Tried to use a texture view but glTextureView is not supported";
            return nullptr;
        }

        osg::ref_ptr<osg::Texture2D> texture2d = new osg::Texture2D;
        texture2d->setSubloadCallback(new Texture2DViewSubloadCallback(textureArray, layer));
        texture2d->setTextureSize(textureArray->getTextureWidth(), textureArray->getTextureHeight());
        texture2d->setBorderColor(textureArray->getBorderColor());
        texture2d->setBorderWidth(textureArray->getBorderWidth());
        texture2d->setLODBias(textureArray->getLODBias());
        texture2d->setFilter(osg::Texture::FilterParameter::MAG_FILTER, textureArray->getFilter(osg::Texture::FilterParameter::MAG_FILTER));
        texture2d->setFilter(osg::Texture::FilterParameter::MIN_FILTER, textureArray->getFilter(osg::Texture::FilterParameter::MIN_FILTER));
        texture2d->setInternalFormat(textureArray->getInternalFormat());
        texture2d->setNumMipmapLevels(textureArray->getNumMipmapLevels());
        return texture2d;
    }

    MultiviewFramebuffer::MultiviewFramebuffer(int width, int height, int samples)
        : mWidth(width)
        , mHeight(height)
        , mSamples(samples)
        , mMultiview(getMultiview())
        , mFbo{ new osg::FrameBufferObject }
        , mMsaaFbo{ new osg::FrameBufferObject }
        , mLayerFbo{ new osg::FrameBufferObject, new osg::FrameBufferObject }
    {
    }

    MultiviewFramebuffer::~MultiviewFramebuffer()
    {
    }

    void MultiviewFramebuffer::attachColorComponent(GLint sourceFormat, GLint sourceType, GLint internalFormat)
    {
        if (mMultiview)
        {
#ifdef OSG_HAS_MULTIVIEW
            if (mSamples > 1)
            {
                mMultiviewMsaaColorTexture = createTextureMsaaArray(sourceFormat, sourceType, internalFormat);
                mMsaaFbo->setAttachment(osg::Camera::COLOR_BUFFER, osg::FrameBufferAttachment(mMultiviewMsaaColorTexture, osg::Camera::FACE_CONTROLLED_BY_MULTIVIEW_SHADER, 0));
            }
            mMultiviewColorTexture = createTextureArray(sourceFormat, sourceType, internalFormat);
            mFbo->setAttachment(osg::Camera::COLOR_BUFFER, osg::FrameBufferAttachment(mMultiviewColorTexture, osg::Camera::FACE_CONTROLLED_BY_MULTIVIEW_SHADER, 0));
            for (unsigned i = 0; i < 2; i++)
            {
                mColorTexture[i] = createTextureView_Texture2DFromTexture2DArray(mMultiviewColorTexture.get(), i);
                mLayerFbo[i]->setAttachment(osg::Camera::COLOR_BUFFER, osg::FrameBufferAttachment(mColorTexture[i]));
            }
#endif
        }
        else
        {
            for (unsigned i = 0; i < 2; i++)
            {
                if (mSamples > 1)
                {
                    mMsaaColorTexture[i] = createTextureMsaa(sourceFormat, sourceType, internalFormat);
                }
                mColorTexture[i] = createTexture(sourceFormat, sourceType, internalFormat);
                mLayerFbo[i]->setAttachment(osg::Camera::COLOR_BUFFER, osg::FrameBufferAttachment(mColorTexture[i]));
            }
        }
    }

    void MultiviewFramebuffer::attachDepthComponent(GLint sourceFormat, GLint sourceType, GLint internalFormat)
    {
        if (mMultiview)
        {
#ifdef OSG_HAS_MULTIVIEW
            if (mSamples > 1)
            {
                mMultiviewMsaaDepthTexture = createTextureMsaaArray(sourceFormat, sourceType, internalFormat);
                mMsaaFbo->setAttachment(osg::Camera::DEPTH_BUFFER, osg::FrameBufferAttachment(mMultiviewMsaaDepthTexture, osg::Camera::FACE_CONTROLLED_BY_MULTIVIEW_SHADER, 0));
            }
            mMultiviewDepthTexture = createTextureArray(sourceFormat, sourceType, internalFormat);
            mFbo->setAttachment(osg::Camera::DEPTH_BUFFER, osg::FrameBufferAttachment(mMultiviewDepthTexture, osg::Camera::FACE_CONTROLLED_BY_MULTIVIEW_SHADER, 0));
            for (unsigned i = 0; i < 2; i++)
            {
                mDepthTexture[i] = createTextureView_Texture2DFromTexture2DArray(mMultiviewDepthTexture.get(), i);
                mLayerFbo[i]->setAttachment(osg::Camera::DEPTH_BUFFER, osg::FrameBufferAttachment(mDepthTexture[i]));
            }
#endif
        }
        else
        {
            for (unsigned i = 0; i < 2; i++)
            {
                if (mSamples > 1)
                {
                    mMsaaDepthTexture[i] = createTextureMsaa(sourceFormat, sourceType, internalFormat);
                }
                mDepthTexture[i] = createTexture(sourceFormat, sourceType, internalFormat);
                mLayerFbo[i]->setAttachment(osg::Camera::DEPTH_BUFFER, osg::FrameBufferAttachment(mDepthTexture[i]));
            }
        }
    }

    osg::FrameBufferObject* MultiviewFramebuffer::fbo()
    {
        return mFbo;
    }

    osg::FrameBufferObject* MultiviewFramebuffer::msaaFbo()
    {
        return mMsaaFbo;
    }

    osg::FrameBufferObject* MultiviewFramebuffer::layerFbo(int i)
    {
        return mLayerFbo[i];
    }

    osg::Texture2D* MultiviewFramebuffer::layerColorBuffer(int i)
    {
        return mColorTexture[i];
    }

    osg::Texture2D* MultiviewFramebuffer::layerDepthBuffer(int i)
    {
        return mDepthTexture[i];
    }
    void MultiviewFramebuffer::attachTo(osg::Camera* camera)
    {
#ifdef OSG_HAS_MULTIVIEW
        if (mMultiview)
        {
            if (mSamples > 1)
            {
                if (mMultiviewMsaaColorTexture)
                {
                    camera->attach(osg::Camera::COLOR_BUFFER, mMultiviewMsaaColorTexture, 0, osg::Camera::FACE_CONTROLLED_BY_MULTIVIEW_SHADER, false, mSamples);
                    camera->getBufferAttachmentMap()[osg::Camera::COLOR_BUFFER]._internalFormat = mMultiviewMsaaColorTexture->getInternalFormat();
                    camera->getBufferAttachmentMap()[osg::Camera::COLOR_BUFFER]._mipMapGeneration = false;
                }
                if (mMultiviewMsaaDepthTexture)
                {
                    camera->attach(osg::Camera::DEPTH_BUFFER, mMultiviewMsaaDepthTexture, 0, osg::Camera::FACE_CONTROLLED_BY_MULTIVIEW_SHADER, false, mSamples);
                    camera->getBufferAttachmentMap()[osg::Camera::DEPTH_BUFFER]._internalFormat = mMultiviewMsaaDepthTexture->getInternalFormat();
                    camera->getBufferAttachmentMap()[osg::Camera::DEPTH_BUFFER]._mipMapGeneration = false;
                }
            }
            else
            {
                if (mMultiviewColorTexture)
                {
                    camera->attach(osg::Camera::COLOR_BUFFER, mMultiviewColorTexture, 0, osg::Camera::FACE_CONTROLLED_BY_MULTIVIEW_SHADER, false);
                    camera->getBufferAttachmentMap()[osg::Camera::COLOR_BUFFER]._internalFormat = mMultiviewColorTexture->getInternalFormat();
                    camera->getBufferAttachmentMap()[osg::Camera::COLOR_BUFFER]._mipMapGeneration = false;
                }
                if (mMultiviewDepthTexture)
                {
                    camera->attach(osg::Camera::DEPTH_BUFFER, mMultiviewDepthTexture, 0, osg::Camera::FACE_CONTROLLED_BY_MULTIVIEW_SHADER, false);
                    camera->getBufferAttachmentMap()[osg::Camera::DEPTH_BUFFER]._internalFormat = mMultiviewDepthTexture->getInternalFormat();
                    camera->getBufferAttachmentMap()[osg::Camera::DEPTH_BUFFER]._mipMapGeneration = false;
                }
            }
            camera->setRenderTargetImplementation(osg::Camera::FRAME_BUFFER_OBJECT);
        }
#endif
    }

    osg::Texture2D* MultiviewFramebuffer::createTexture(GLint sourceFormat, GLint sourceType, GLint internalFormat)
    {
        osg::Texture2D* texture = new osg::Texture2D;
        texture->setTextureSize(mWidth, mHeight);
        texture->setSourceFormat(sourceFormat);
        texture->setSourceType(sourceType);
        texture->setInternalFormat(internalFormat);
        texture->setFilter(osg::Texture::MIN_FILTER, osg::Texture::LINEAR);
        texture->setFilter(osg::Texture::MAG_FILTER, osg::Texture::LINEAR);
        texture->setWrap(osg::Texture::WRAP_S, osg::Texture::CLAMP_TO_EDGE);
        texture->setWrap(osg::Texture::WRAP_T, osg::Texture::CLAMP_TO_EDGE);
        texture->setWrap(osg::Texture::WRAP_R, osg::Texture::CLAMP_TO_EDGE);
        return texture;
    }

    osg::Texture2DMultisample* MultiviewFramebuffer::createTextureMsaa(GLint sourceFormat, GLint sourceType, GLint internalFormat)
    {
        osg::Texture2DMultisample* texture = new osg::Texture2DMultisample;
        texture->setTextureSize(mWidth, mHeight);
        texture->setNumSamples(mSamples);
        texture->setSourceFormat(sourceFormat);
        texture->setSourceType(sourceType);
        texture->setInternalFormat(internalFormat);
        texture->setFilter(osg::Texture::MIN_FILTER, osg::Texture::LINEAR);
        texture->setFilter(osg::Texture::MAG_FILTER, osg::Texture::LINEAR);
        texture->setWrap(osg::Texture::WRAP_S, osg::Texture::CLAMP_TO_EDGE);
        texture->setWrap(osg::Texture::WRAP_T, osg::Texture::CLAMP_TO_EDGE);
        texture->setWrap(osg::Texture::WRAP_R, osg::Texture::CLAMP_TO_EDGE);
        return texture;
    }

    osg::Texture2DArray* MultiviewFramebuffer::createTextureArray(GLint sourceFormat, GLint sourceType, GLint internalFormat)
    {
        osg::Texture2DArray* textureArray = new osg::Texture2DArray;
        textureArray->setTextureSize(mWidth, mHeight, 2);
        textureArray->setSourceFormat(sourceFormat);
        textureArray->setSourceType(sourceType);
        textureArray->setInternalFormat(internalFormat);
        textureArray->setFilter(osg::Texture::MIN_FILTER, osg::Texture::LINEAR);
        textureArray->setFilter(osg::Texture::MAG_FILTER, osg::Texture::LINEAR);
        textureArray->setWrap(osg::Texture::WRAP_S, osg::Texture::CLAMP_TO_EDGE);
        textureArray->setWrap(osg::Texture::WRAP_T, osg::Texture::CLAMP_TO_EDGE);
        textureArray->setWrap(osg::Texture::WRAP_R, osg::Texture::CLAMP_TO_EDGE);
        return textureArray;
    }

    osg::Texture2DMultisampleArray* MultiviewFramebuffer::createTextureMsaaArray(GLint sourceFormat, GLint sourceType, GLint internalFormat)
    {
        osg::Texture2DMultisampleArray* textureArray = new osg::Texture2DMultisampleArray;
        textureArray->setTextureSize(mWidth, mHeight, 2);
        textureArray->setNumSamples(mSamples);
        textureArray->setSourceFormat(sourceFormat);
        textureArray->setSourceType(sourceType);
        textureArray->setInternalFormat(internalFormat);
        textureArray->setFilter(osg::Texture::MIN_FILTER, osg::Texture::LINEAR);
        textureArray->setFilter(osg::Texture::MAG_FILTER, osg::Texture::LINEAR);
        textureArray->setWrap(osg::Texture::WRAP_S, osg::Texture::CLAMP_TO_EDGE);
        textureArray->setWrap(osg::Texture::WRAP_T, osg::Texture::CLAMP_TO_EDGE);
        textureArray->setWrap(osg::Texture::WRAP_R, osg::Texture::CLAMP_TO_EDGE);
        return textureArray;
    }
}
