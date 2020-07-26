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
        VrShadow();

        void configureShadowsForCamera(osg::Camera* camera, bool master);

        void updateShadowConfig(osg::View& view);

    private:
        osg::ref_ptr<SharedShadowMapConfig> mMasterConfig;
        osg::ref_ptr<SharedShadowMapConfig> mSlaveConfig;
    };
}

#endif
