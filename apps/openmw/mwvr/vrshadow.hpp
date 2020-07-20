#ifndef MWVR_VRSHADOW_H
#define MWVR_VRSHADOW_H

#include <osg/Camera>
#include <osgViewer/Viewer>

#include <components/sceneutil/mwshadowtechnique.hpp>

namespace MWVR
{

    class UpdateShadowMapSlaveCallback : public osg::View::Slave::UpdateSlaveCallback
    {
    public:
        void updateSlave(osg::View& view, osg::View::Slave& slave) override;
    };

    class VrShadow
    {
        using SharedShadowMapConfig = SceneUtil::MWShadowTechnique::SharedShadowMapConfig;
    public:
        VrShadow(osgViewer::Viewer* viewer, int renderOrder = 0);

        void configureShadowsForCamera(osg::Camera* camera);

        void configureShadows(bool enabled);

    private:
        osgViewer::Viewer* mViewer;
        int mRenderOrder;
        osg::ref_ptr<osg::Camera> mShadowMapCamera;
        osg::ref_ptr< UpdateShadowMapSlaveCallback > mUpdateCallback;
        osg::ref_ptr<SharedShadowMapConfig> mMasterConfig;
        osg::ref_ptr<SharedShadowMapConfig> mSlaveConfig;
    };
}

#endif
