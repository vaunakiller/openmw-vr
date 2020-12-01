#ifndef MWVR_VRANIMATION_H
#define MWVR_VRANIMATION_H

#include "../mwrender/npcanimation.hpp"
#include "../mwrender/renderingmanager.hpp"
#include "openxrmanager.hpp"
#include "vrsession.hpp"

namespace MWVR
{

    class HandController;
    class FingerController;
    class ForearmController;

    /// Subclassing NpcAnimation to implement VR related behaviour
    class VRAnimation : public MWRender::NpcAnimation
    {
    protected:
        virtual void addControllers();

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
            bool disableSounds, std::shared_ptr<VRSession> xrSession);
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

        /// Overrides finger animations to point forward
        void setFingerPointingMode(bool enabled);

        /// @return Whether animation is currently in finger pointing mode
        bool fingerPointingMode() const { return mFingerPointingMode; }

        /// @return true if it is possible to place on object where the player is currently pointing
        bool canPlaceObject();

        /// @return pointer to the object the player's melee weapon is currently intersecting.
        const MWRender::RayResult& getPointerTarget() const;

        /// Update what object this vr animation is currently pointing at.
        void updatePointerTarget();

        /// @return whatever ref is the current pointer target, if any
        MWWorld::Ptr getTarget(const std::string& directorNode);

        /// @return world transform that yields the position and orientation of the current weapon
        osg::Matrix getWeaponTransformMatrix() const;

    protected:
        static osg::ref_ptr<osg::Geometry> createPointerGeometry(void);

        float getVelocity(const std::string& groupname) const override;

    protected:
        std::shared_ptr<VRSession> mSession;
        osg::ref_ptr<ForearmController> mForearmControllers[2];
        osg::ref_ptr<HandController> mHandControllers[2];
        osg::ref_ptr<FingerController> mIndexFingerControllers[2];
        osg::ref_ptr<osg::MatrixTransform> mModelOffset;

        bool mFingerPointingMode{ false };
        osg::ref_ptr<osg::Geometry> mPointerGeometry{ nullptr };
        osg::ref_ptr<osg::MatrixTransform> mPointerRescale{ nullptr };
        osg::ref_ptr<osg::MatrixTransform> mPointerTransform{ nullptr };
        osg::ref_ptr<osg::MatrixTransform> mWeaponDirectionTransform{ nullptr };
        osg::ref_ptr<osg::MatrixTransform> mWeaponPointerTransform{ nullptr };
        MWRender::RayResult mPointerTarget{};
        float mDistanceToPointerTarget{ -1.f };
    };

}

#endif
