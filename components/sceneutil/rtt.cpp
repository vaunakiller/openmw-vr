#include "rtt.hpp"
#include "util.hpp"

#include <osg/Node>
#include <osg/NodeVisitor>
#include <osg/Texture2D>
#include <osg/Texture2DArray>
#include <osgUtil/CullVisitor>

#include <components/sceneutil/nodecallback.hpp>
#include <components/settings/settings.hpp>
#include <components/misc/stereo.hpp>
#include <components/debug/debuglog.hpp>

namespace SceneUtil
{
    class CullCallback : public SceneUtil::NodeCallback<CullCallback, RTTNode*, osgUtil::CullVisitor*>
    {
    public:

        void operator()(RTTNode* node, osgUtil::CullVisitor* cv)
        {
            node->cull(cv);
        }
    };

    class CreateTextureViewsCallback : public osg::Camera::DrawCallback
    {
    public:
        CreateTextureViewsCallback(std::shared_ptr<RTTNode::ViewDependentData> vdd, GLint colorFormat, GLint depthFormat)
            : mVdd(vdd)
            , mColorFormat(colorFormat)
            , mDepthFormat(depthFormat)
        {
        }

        void operator () (osg::RenderInfo& renderInfo) const override
        {
            if (mDone)
                return;

            auto* state = renderInfo.getState();
            auto contextId = state->getContextID();
            auto* gl = osg::GLExtensions::Get(contextId, false);

            // The stereo manager checks for texture view support before enabling multiview, so this check is probably redundant.
            if (gl->glTextureView)
            {
                auto sourceTexture = mVdd->mCamera->getBufferAttachmentMap()[osg::Camera::COLOR_BUFFER]._texture;
                auto sourceTextureObject = sourceTexture->getTextureObject(contextId);
                auto sourceId = sourceTextureObject->_id;

                GLuint targetId = 0;
                glGenTextures(1, &targetId);
                gl->glTextureView(targetId, GL_TEXTURE_2D, sourceId, sourceTextureObject->_profile._internalFormat, 0, 1, 0, 1);

                auto targetTexture = mVdd->mColorTexture;
                auto targetTextureObject = new osg::Texture::TextureObject(targetTexture, targetId, GL_TEXTURE_2D);
                targetTexture->setTextureObject(contextId, targetTextureObject);
            }
            else {
                Log(Debug::Error) << "Requested glTextureView but glTextureView is not supported";
            }

            mDone = true;

            // Don't hold on to the reference as we're not gonna run again.
            // This should not cause an immediate deletion of the camera (and consequentially this callback)
            // as the stategraph/renderbin holds a reference.
            mVdd = nullptr;
        }

        mutable bool mDone = false;
        // Use a weak pointer to prevent cyclic reference
        mutable std::shared_ptr<RTTNode::ViewDependentData> mVdd;
        const GLint mColorFormat;
        const GLint mDepthFormat;
    };

    RTTNode::RTTNode(uint32_t textureWidth, uint32_t textureHeight, int renderOrderNum, StereoAwareness stereoAwareness)
        : mTextureWidth(textureWidth)
        , mTextureHeight(textureHeight)
        , mColorBufferInternalFormat(GL_RGB)
        , mDepthBufferInternalFormat(GL_DEPTH_COMPONENT)
        , mRenderOrderNum(renderOrderNum)
        , mStereoAwareness(stereoAwareness)
    {
        addCullCallback(new CullCallback);
        setCullingActive(false);
    }

    RTTNode::~RTTNode()
    {
        for (auto& vdd : mViewDependentDataMap)
        {
            auto camera = vdd.second->mCamera;
            if (camera)
            {
                camera->removeChildren(0, camera->getNumChildren());
            }
        }
        mViewDependentDataMap.clear();
    }

    void RTTNode::cull(osgUtil::CullVisitor* cv)
    {
        auto frameNumber = cv->getFrameStamp()->getFrameNumber();
        auto* vdd = getViewDependentData(cv);
        if (frameNumber > vdd->mFrameNumber)
        {
            apply(vdd->mCamera);
            vdd->mCamera->accept(*cv);
        }
        vdd->mFrameNumber = frameNumber;
    }

    void RTTNode::setColorBufferInternalFormat(GLint internalFormat)
    {
        mColorBufferInternalFormat = internalFormat;
    }

    void RTTNode::setDepthBufferInternalFormat(GLint internalFormat)
    {
        mDepthBufferInternalFormat = internalFormat;
    }

    bool RTTNode::shouldDoPerViewMapping()
    {
        if(mStereoAwareness != StereoAwareness::Aware)
            return false;
        if (!Misc::StereoView::instance().getMultiview())
            return true;
        return false;
    }

    bool RTTNode::shouldDoTextureArray()
    {
        if (mStereoAwareness == StereoAwareness::Unaware)
            return false;
        if (Misc::StereoView::instance().getMultiview())
            return true;
        return false;
    }

    bool RTTNode::shouldDoTextureView()
    {
        if (mStereoAwareness != StereoAwareness::StereoUnawareMultiViewAware)
            return false;
        if (Misc::StereoView::instance().getMultiview())
            return true;
        return false;
    }

    osg::Texture2DArray* RTTNode::createTextureArray(GLint internalFormat)
    {
        osg::Texture2DArray* textureArray = new osg::Texture2DArray;
        textureArray->setTextureSize(mTextureWidth, mTextureHeight, 2);
        textureArray->setInternalFormat(internalFormat);
        textureArray->setFilter(osg::Texture::MIN_FILTER, osg::Texture::LINEAR);
        textureArray->setFilter(osg::Texture::MAG_FILTER, osg::Texture::LINEAR);
        textureArray->setWrap(osg::Texture::WRAP_S, osg::Texture::CLAMP_TO_EDGE);
        textureArray->setWrap(osg::Texture::WRAP_T, osg::Texture::CLAMP_TO_EDGE);
        textureArray->setWrap(osg::Texture::WRAP_R, osg::Texture::CLAMP_TO_EDGE);
        return textureArray;
    }

    osg::Texture2D* RTTNode::createTexture(GLint internalFormat)
    {
        osg::Texture2D* texture = new osg::Texture2D;
        texture->setTextureSize(mTextureWidth, mTextureHeight);
        texture->setInternalFormat(internalFormat);
        texture->setFilter(osg::Texture::MIN_FILTER, osg::Texture::LINEAR);
        texture->setFilter(osg::Texture::MAG_FILTER, osg::Texture::LINEAR);
        texture->setWrap(osg::Texture::WRAP_S, osg::Texture::CLAMP_TO_EDGE);
        texture->setWrap(osg::Texture::WRAP_T, osg::Texture::CLAMP_TO_EDGE);
        texture->setWrap(osg::Texture::WRAP_R, osg::Texture::CLAMP_TO_EDGE);
        return texture;
    }

    osg::Texture* RTTNode::getColorTexture(osgUtil::CullVisitor* cv)
    {
        return getViewDependentData(cv)->mColorTexture;
    }

    osg::Texture* RTTNode::getDepthTexture(osgUtil::CullVisitor* cv)
    {
        return getViewDependentData(cv)->mDepthTexture;
    }

    osg::Camera* RTTNode::getCamera(osgUtil::CullVisitor* cv)
    {
        return getViewDependentData(cv)->mCamera;
    }

    RTTNode::ViewDependentData* RTTNode::getViewDependentData(osgUtil::CullVisitor* cv)
    {
        if (!shouldDoPerViewMapping())
            // Always setting it to null is an easy way to disable per-view mapping when mDoPerViewMapping is false.
            // This is safe since the visitor is never dereferenced.
            cv = nullptr;

        if (mViewDependentDataMap.count(cv) == 0)
        {
            auto camera = new osg::Camera();
            auto vdd = std::make_shared<ViewDependentData>();
            mViewDependentDataMap[cv] = vdd;
            mViewDependentDataMap[cv]->mCamera = camera;

            camera->setRenderOrder(osg::Camera::PRE_RENDER, mRenderOrderNum);
            camera->setClearMask(GL_DEPTH_BUFFER_BIT | GL_COLOR_BUFFER_BIT);
            camera->setRenderTargetImplementation(osg::Camera::FRAME_BUFFER_OBJECT);
            camera->setViewport(0, 0, mTextureWidth, mTextureHeight);

            setDefaults(camera);

            if (camera->getBufferAttachmentMap().count(osg::Camera::COLOR_BUFFER))
                vdd->mColorTexture = camera->getBufferAttachmentMap()[osg::Camera::COLOR_BUFFER]._texture;
            if (camera->getBufferAttachmentMap().count(osg::Camera::DEPTH_BUFFER))
                vdd->mDepthTexture = camera->getBufferAttachmentMap()[osg::Camera::DEPTH_BUFFER]._texture;

#ifdef OSG_HAS_MULTIVIEW
            if (shouldDoTextureArray())
            {
                // Create any buffer attachments not added in setDefaults
                if (camera->getBufferAttachmentMap().count(osg::Camera::COLOR_BUFFER) == 0)
                {
                    vdd->mColorTexture = createTextureArray(mColorBufferInternalFormat);
                    
                    camera->attach(osg::Camera::COLOR_BUFFER, vdd->mColorTexture, 0, osg::Camera::FACE_CONTROLLED_BY_MULTIVIEW_SHADER);
                    SceneUtil::attachAlphaToCoverageFriendlyFramebufferToCamera(camera, osg::Camera::COLOR_BUFFER, vdd->mColorTexture, 0, osg::Camera::FACE_CONTROLLED_BY_MULTIVIEW_SHADER);
                }

                if (camera->getBufferAttachmentMap().count(osg::Camera::DEPTH_BUFFER) == 0)
                {
                    vdd->mDepthTexture = createTextureArray(mDepthBufferInternalFormat);

                    camera->attach(osg::Camera::DEPTH_BUFFER, vdd->mDepthTexture, 0, osg::Camera::FACE_CONTROLLED_BY_MULTIVIEW_SHADER);
                }

                if (shouldDoTextureView())
                {
                    // In this case, shaders being set to multiview forces us to render to a multiview framebuffer even though we don't need to.
                    // To make the result compatible with the intended use, we have to return Texture2D views into the Texture2DArray.
                    camera->setFinalDrawCallback(new CreateTextureViewsCallback(vdd, mColorBufferInternalFormat, mDepthBufferInternalFormat));

                    // References are kept alive by the camera's attachment buffers, so just override the vdd textures with the views.
                    vdd->mColorTexture = createTexture(mColorBufferInternalFormat);
                    vdd->mDepthTexture = createTexture(mDepthBufferInternalFormat);
                }
            }
            else
#endif
            {
                // Create any buffer attachments not added in setDefaults
                if (camera->getBufferAttachmentMap().count(osg::Camera::COLOR_BUFFER) == 0)
                {
                    vdd->mColorTexture = createTexture(mColorBufferInternalFormat);
                    camera->attach(osg::Camera::COLOR_BUFFER, vdd->mColorTexture);
                    SceneUtil::attachAlphaToCoverageFriendlyFramebufferToCamera(camera, osg::Camera::COLOR_BUFFER, vdd->mColorTexture);
                }

                if (camera->getBufferAttachmentMap().count(osg::Camera::DEPTH_BUFFER) == 0)
                {
                    vdd->mDepthTexture = createTexture(mDepthBufferInternalFormat);
                    camera->attach(osg::Camera::DEPTH_BUFFER, vdd->mDepthTexture);
                }
            }
        }

        return mViewDependentDataMap[cv].get();
    }
}
