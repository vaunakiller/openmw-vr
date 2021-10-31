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

    RTTNode::RTTNode(uint32_t textureWidth, uint32_t textureHeight, int renderOrderNum, StereoAwareness stereoAwareness)
        : mTextureWidth(textureWidth)
        , mTextureHeight(textureHeight)
        , mRenderOrderNum(renderOrderNum)
        , mStereoAwareness(stereoAwareness)
    {
        addCullCallback(new CullCallback);
        setCullingActive(false);
    }

    RTTNode::~RTTNode()
    {
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

    bool RTTNode::shouldDoPerViewMapping()
    {
        if(mStereoAwareness == StereoAwareness::Unaware)
            return false;
        if (Misc::StereoView::instance().getTechnique() == Misc::StereoView::Technique::BruteForce)
            return true;
        return false;
    }

    bool RTTNode::shouldDoTextureArray()
    {
        if (mStereoAwareness == StereoAwareness::Unaware)
            return false;
        if (Misc::StereoView::instance().getTechnique() == Misc::StereoView::Technique::OVR_MultiView2)
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
        return getViewDependentData(cv)->mCamera->getBufferAttachmentMap()[osg::Camera::COLOR_BUFFER]._texture;
    }

    osg::Texture* RTTNode::getDepthTexture(osgUtil::CullVisitor* cv)
    {
        return getViewDependentData(cv)->mCamera->getBufferAttachmentMap()[osg::Camera::DEPTH_BUFFER]._texture;
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
            mViewDependentDataMap[cv].reset(new ViewDependentData);
            mViewDependentDataMap[cv]->mCamera = camera;

            camera->setRenderOrder(osg::Camera::PRE_RENDER, mRenderOrderNum);
            camera->setClearMask(GL_DEPTH_BUFFER_BIT | GL_COLOR_BUFFER_BIT);
            camera->setRenderTargetImplementation(osg::Camera::FRAME_BUFFER_OBJECT);
            camera->setViewport(0, 0, mTextureWidth, mTextureHeight);

            setDefaults(mViewDependentDataMap[cv]->mCamera.get());

#ifdef OSG_HAS_MULTIVIEW
            if (shouldDoTextureArray())
            {
                // Create any buffer attachments not added in setDefaults
                if (camera->getBufferAttachmentMap().count(osg::Camera::COLOR_BUFFER) == 0)
                {
                    auto colorBuffer = createTextureArray(GL_RGB);
                    camera->attach(osg::Camera::COLOR_BUFFER, colorBuffer, 0, osg::Camera::FACE_CONTROLLED_BY_MULTIVIEW_SHADER);
                    //SceneUtil::attachAlphaToCoverageFriendlyFramebufferToCamera(camera, osg::Camera::COLOR_BUFFER, colorBuffer);
                }

                if (camera->getBufferAttachmentMap().count(osg::Camera::DEPTH_BUFFER) == 0)
                {
                    auto depthBuffer = createTextureArray(GL_DEPTH_COMPONENT);

                    camera->attach(osg::Camera::DEPTH_BUFFER, depthBuffer, 0, osg::Camera::FACE_CONTROLLED_BY_MULTIVIEW_SHADER);
                }
            }
            else
#endif
            {
                // Create any buffer attachments not added in setDefaults
                if (camera->getBufferAttachmentMap().count(osg::Camera::COLOR_BUFFER) == 0)
                {
                    auto colorBuffer = createTexture(GL_RGB);
                    camera->attach(osg::Camera::COLOR_BUFFER, colorBuffer);
                    SceneUtil::attachAlphaToCoverageFriendlyFramebufferToCamera(camera, osg::Camera::COLOR_BUFFER, colorBuffer);
                }

                if (camera->getBufferAttachmentMap().count(osg::Camera::DEPTH_BUFFER) == 0)
                {
                    auto depthBuffer = createTexture(GL_DEPTH_COMPONENT);
                    camera->attach(osg::Camera::DEPTH_BUFFER, depthBuffer);
                }
            }
        }

        return mViewDependentDataMap[cv].get();
    }
}
