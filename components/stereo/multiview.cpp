#include "multiview.hpp"

#include <osg/GLExtensions>
#include <osg/Texture2D>
#include <osg/Texture2DArray>

#include <components/settings/settings.hpp>
#include <components/debug/debuglog.hpp>

namespace Stereo
{
    namespace
    {
        bool getMultiviewSupported(unsigned int contextID)
        {
#ifdef OSG_HAS_MULTIVIEW
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

            if (!osg::isGLExtensionOrVersionSupported(contextID, "ARB_texture_view", 4.3))
            {
                Log(Debug::Verbose) << "Disabling Multiview (opengl extension \"ARB_texture_view\" not supported)";
                return false;
            }

            Log(Debug::Verbose) << "Enabling Multiview";

            return true;
#else
            return false;
#endif
        }
    }

    bool getMultiview(unsigned int contextID)
    {
        static bool multiView = getMultiviewSupported(contextID);
        return multiView;
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
        if (gl->glTextureView)
        {
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

            // OSG already bound this texture ID and gave it a target.
            // Delete it and make a new one.
            glBindTexture(GL_TEXTURE_2D, 0);
            glDeleteTextures(1, &targetTextureObject->_id);
            glGenTextures(1, &targetTextureObject->_id);

            auto sourceId = sourceTextureObject->_id;
            auto targetId = targetTextureObject->_id;
            auto internalFormat = sourceTextureObject->_profile._internalFormat;
            auto levels = std::max(1, sourceTextureObject->_profile._numMipmapLevels);
            gl->glTextureView(targetId, GL_TEXTURE_2D, sourceId, internalFormat, 0, levels, mLayer, 1);
            glBindTexture(GL_TEXTURE_2D, targetId);
        }
        else {
            Log(Debug::Error) << "Texture2DViewSubloadCallback: Tried to use a texture view but glTextureView is not supported";
        }
    }

    void Texture2DViewSubloadCallback::subload(const osg::Texture2D& texture, osg::State& state) const
    {
        // Nothing to do
    }

    osg::ref_ptr<osg::Texture2D> createTextureView_Texture2DFromTexture2DArray(osg::Texture2DArray* textureArray, int layer)
    {
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
}
