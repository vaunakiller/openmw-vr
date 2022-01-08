#ifndef MWVR_VRANIMATION_H
#define MWVR_VRANIMATION_H

#include "../mwrender/npcanimation.hpp"
#include "../mwrender/renderingmanager.hpp"
#include <components/vr/trackinglistener.hpp>

namespace MWVR
{
    class HandController;
    class FingerController;
    class TrackingController;
    class UserPointer;

    /// Subclassing NpcAnimation to implement VR related behaviour
    class VRAnimation : public MWRender::NpcAnimation, public VR::TrackingListener
    {
    protected:
        virtual void addControllers() override;

    public:
        /**
         * @param ptr
         * @param disableListener  Don't listen for equipment changes and magic effects. InventoryStore only supports
         *                         one listener at a time, so you shouldn't do this if creating several NpcAnimations
         *                         for the same Ptr, eg preview dolls for the player.
         *                         Those need to be manually rendered anyway.
         * @param disableSounds    Same as \a disableListener but for playing items sounds
         * @param xrSession        The XR session that shall be used to track limbs
         */
        VRAnimation(const MWWorld::Ptr& ptr, osg::ref_ptr<osg::Group> parentNode, Resource::ResourceSystem* resourceSystem,
            bool disableSounds);
        virtual ~VRAnimation();

        /// Overridden to always be false
        void enableHeadAnimation(bool enable) override;

        /// Overridden to always be false
        void setAccurateAiming(bool enabled) override;

        /// Overridden, implementation tbd
        osg::Vec3f runAnimation(float timepassed) override;

        /// Overriden to always be a variant of VM_VR*
        void setViewMode(ViewMode viewMode) override;

        /// Overriden to include VR modifications
        void updateParts() override;

        /// @return world transform that yields the position and orientation of the current weapon
        osg::Matrix getWeaponTransformMatrix() const;

    protected:

        float getVelocity(const std::string& groupname) const override;

        void onTrackingUpdated(VR::TrackingManager& manager, VR::DisplayTime predictedDisplayTime) override;

    protected:
        typedef std::unordered_map<std::string, std::unique_ptr<TrackingController>, Misc::StringUtils::CiHash, Misc::StringUtils::CiEqual> TrackingControllerMap;
        TrackingControllerMap mVrControllers;
        osg::ref_ptr<HandController> mHandControllers[2];
        osg::ref_ptr<FingerController> mLeftIndexFingerControllers[2];
        osg::ref_ptr<FingerController> mRightIndexFingerControllers[2];
        osg::ref_ptr<osg::MatrixTransform> mModelOffset;

        std::shared_ptr<UserPointer> mUserPointer;
        osg::ref_ptr<osg::MatrixTransform> mWeaponDirectionTransform{ nullptr };
        osg::ref_ptr<osg::MatrixTransform> mWeaponPointerTransform{ nullptr };
    };

}

#endif
