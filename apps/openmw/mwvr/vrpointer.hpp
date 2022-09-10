#ifndef MWVR_POINTER_H
#define MWVR_POINTER_H

#include "../mwrender/npcanimation.hpp"
#include "../mwrender/renderingmanager.hpp"

#include <components/vr/trackinglistener.hpp>

namespace SceneUtil
{
    class PositionAttitudeTransform;
}

namespace MWVR
{
    //! Controls the beam used to target/select objects.
    class UserPointer
    {

    public:
        UserPointer();
        ~UserPointer();

        void updatePointerTarget();
        const MWRender::RayResult& getPointerRay() const;
        bool canPlaceObject() const;
        void setSource(VR::VRPath source);
        bool enabled() const { return !!mSource; };
        float distanceToPointerTarget() const { return mDistanceToPointerTarget; }
        void activate();

    private:
        osg::ref_ptr<osg::Geometry> createPointerGeometry();

        osg::ref_ptr<osg::Geometry> mPointerGeometry;
        osg::ref_ptr<SceneUtil::PositionAttitudeTransform> mPointerPAT;

        osg::ref_ptr<osg::Transform> mSource;
        VR::VRPath mSourcePath = 0;
        osg::ref_ptr<osg::Group> mRoot;

        MWRender::RayResult mPointerRay = {};
        osg::ref_ptr<osg::Node> mPointerTarget;
        float mDistanceToPointerTarget = -1.f;
        bool mCanPlaceObject = false;
        bool mLeftHandEnabled = true;
        bool mRightHandEnabled = true;
    };
}

#endif
