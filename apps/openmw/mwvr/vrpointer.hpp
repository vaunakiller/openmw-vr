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
        const MWRender::RayResult& getPointerTarget() const;
        bool canPlaceObject() const;
        void setParent(osg::Group* group);
        void setEnabled(bool enabled);
        float distanceToPointerTarget() const { return mDistanceToPointerTarget; }
    protected:
        void onTrackingUpdated(VR::TrackingManager& manager, VR::DisplayTime predictedDisplayTime) override;

    private:
        osg::ref_ptr<osg::Geometry> createPointerGeometry();

        osg::ref_ptr<osg::Geometry> mPointerGeometry{ nullptr };
        osg::ref_ptr<osg::MatrixTransform> mPointerRescale{ nullptr };
        osg::ref_ptr<osg::MatrixTransform> mPointerTransform{ nullptr };

        osg::ref_ptr<osg::Group> mParent{ nullptr };
        osg::ref_ptr<osg::Group> mRoot{ nullptr };
        VR::VRPath mHandPath;

        bool mEnabled;
        MWRender::RayResult mPointerTarget{};
        float mDistanceToPointerTarget{ -1.f };
        bool mCanPlaceObject{ false };
    };
}

#endif
