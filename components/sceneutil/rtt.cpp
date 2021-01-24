#include "rtt.hpp"

#include <osg/Node>
#include <osg/NodeVisitor>
#include <osg/Texture2D>
#include <osgUtil/CullVisitor>

#include <components/settings/settings.hpp>

namespace SceneUtil
{
    // RTTNode's cull callback
    class CullCallback : public osg::NodeCallback
    {
    public:
        CullCallback(RTTNode* group)
            : mGroup(group) {}

        void operator()(osg::Node* node, osg::NodeVisitor* nv) override
        {
            osgUtil::CullVisitor* cv = static_cast<osgUtil::CullVisitor*>(nv);
            mGroup->cull(cv);
        }
        RTTNode* mGroup;
    };

    // RTT camera 
    class RTTCamera : public osg::Camera
    {
    public:
        RTTCamera()
        {
            setRenderOrder(osg::Camera::PRE_RENDER, 1);
            setClearMask(GL_DEPTH_BUFFER_BIT | GL_COLOR_BUFFER_BIT);
            setRenderTargetImplementation(osg::Camera::FRAME_BUFFER_OBJECT);

            unsigned int rttSize = Settings::Manager::getInt("rtt size", "Water");
            setViewport(0, 0, rttSize, rttSize);

            mColorBuffer = new osg::Texture2D;
            mColorBuffer->setTextureSize(rttSize, rttSize);
            mColorBuffer->setWrap(osg::Texture::WRAP_S, osg::Texture::CLAMP_TO_EDGE);
            mColorBuffer->setWrap(osg::Texture::WRAP_T, osg::Texture::CLAMP_TO_EDGE);
            mColorBuffer->setInternalFormat(GL_RGB);
            mColorBuffer->setFilter(osg::Texture::MIN_FILTER, osg::Texture::LINEAR);
            mColorBuffer->setFilter(osg::Texture::MAG_FILTER, osg::Texture::LINEAR);
            attach(osg::Camera::COLOR_BUFFER, mColorBuffer);

            mDepthBuffer = new osg::Texture2D;
            mDepthBuffer->setTextureSize(rttSize, rttSize);
            mDepthBuffer->setSourceFormat(GL_DEPTH_COMPONENT);
            mDepthBuffer->setInternalFormat(GL_DEPTH_COMPONENT24);
            mDepthBuffer->setWrap(osg::Texture::WRAP_S, osg::Texture::CLAMP_TO_EDGE);
            mDepthBuffer->setWrap(osg::Texture::WRAP_T, osg::Texture::CLAMP_TO_EDGE);
            mDepthBuffer->setSourceType(GL_UNSIGNED_INT);
            mDepthBuffer->setFilter(osg::Texture::MIN_FILTER, osg::Texture::LINEAR);
            mDepthBuffer->setFilter(osg::Texture::MAG_FILTER, osg::Texture::LINEAR);
            attach(osg::Camera::DEPTH_BUFFER, mDepthBuffer);
        }

        osg::ref_ptr<osg::Texture2D> mColorBuffer;
        osg::ref_ptr<osg::Texture2D> mDepthBuffer;
    };

    RTTNode::RTTNode(bool doPerViewMapping)
        : mDoPerViewMapping(doPerViewMapping)
    {
        addCullCallback(new CullCallback(this));
    }

    RTTNode::~RTTNode()
    {
    }

    void RTTNode::cull(osgUtil::CullVisitor* cv)
    {
        auto* vdd = getViewDependentData(cv);
        apply(vdd->mCamera);
        vdd->mCamera->accept(*cv);
    }

    osg::Texture2D* RTTNode::getColorTexture(osgUtil::CullVisitor* cv)
    {
        return getViewDependentData(cv)->mCamera->mColorBuffer.get();
    }

    osg::Texture2D* RTTNode::getDepthTexture(osgUtil::CullVisitor* cv)
    {
        return getViewDependentData(cv)->mCamera->mDepthBuffer.get();
    }

    RTTNode::ViewDependentData* RTTNode::getViewDependentData(osgUtil::CullVisitor* cv)
    {
        if (!mDoPerViewMapping)
            // Always setting it to null is an easy way to disable per-view mapping when mDoPerViewMapping is false.
            // This is safe since the visitor is never dereferenced.
            cv = nullptr;

        if (mViewDependentDataMap.count(cv) == 0)
        {
            mViewDependentDataMap[cv].reset(new ViewDependentData);
            mViewDependentDataMap[cv]->mCamera = new RTTCamera();
            setDefaults(mViewDependentDataMap[cv]->mCamera.get());
        }

        return mViewDependentDataMap[cv].get();
    }
}
