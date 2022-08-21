#ifndef MWVR_POINTER_H
#define MWVR_POINTER_H

#include "../mwrender/npcanimation.hpp"
#include "../mwrender/renderingmanager.hpp"

#include <components/vr/trackinglistener.hpp>

namespace MWVR
{
    //! Controls the beam used to target/select objects.
    class UserPointer : public VR::TrackingListener
    {

    public:
        UserPointer(osg::Group* root);
        ~UserPointer();

        void updatePointerTarget();
        const MWRender::RayResult& getPointerRay() const;
        bool canPlaceObject() const;
        void setParent(osg::Group* group);
        void setEnabled(bool enabled);
        void setHandEnabled(bool left, bool right);
        bool enabled() const { return mEnabled; };
        float distanceToPointerTarget() const { return mDistanceToPointerTarget; }
    protected:
        void onTrackingUpdated(VR::TrackingManager& manager, VR::DisplayTime predictedDisplayTime) override;

    private:
        osg::ref_ptr<osg::Geometry> createPointerGeometry();

        osg::ref_ptr<osg::Geometry> mPointerGeometry;
        osg::ref_ptr<osg::MatrixTransform> mPointerRescale;
        osg::ref_ptr<osg::MatrixTransform> mPointerTransform;

        osg::ref_ptr<osg::Group> mParent;
        osg::ref_ptr<osg::Group> mRoot;
        VR::VRPath mLeftHandPath = 0;
        VR::VRPath mRightHandPath = 0;

        bool mEnabled = true;
        MWRender::RayResult mPointerRay = {};
        osg::ref_ptr<osg::Node> mPointerTarget;
        float mDistanceToPointerTarget = -1.f;
        bool mCanPlaceObject = false;
        bool mLeftHandEnabled = true;
        bool mRightHandEnabled = true;
    };
}

#endif
