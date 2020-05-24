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

/// Subclassing NpcAnimation to override behaviours not compatible with VR
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
                 bool disableSounds, std::shared_ptr<VRSession> xrSession );
    virtual ~VRAnimation();

    /// Overridden to always be false
    virtual void enableHeadAnimation(bool enable);

    /// Overridden to always be false
    virtual void setAccurateAiming(bool enabled);

    /// Overridden, implementation tbd
    virtual osg::Vec3f runAnimation(float timepassed);

    /// A relative factor (0-1) that decides if and how much the skeleton should be pitched
    /// to indicate the facing orientation of the character.
    virtual void setPitchFactor(float factor) { mPitchFactor = factor; }

    /// Overriden to always be a variant of VM_VR*
    virtual void setViewMode(ViewMode viewMode);

    /// Overriden to include VR modifications
    virtual void updateParts();

    /// Overrides finger animations to point forward
    void setPointForward(bool enabled);
    bool isPointingForward(void) const { return mIsPointingForward; }

    bool canPlaceObject();
    ///< @return true if it is possible to place on object where the player is currently pointing

    const MWRender::RayResult& getPointerTarget() const;
    ///< @return pointer to the object the player's melee weapon is currently intersecting.

    void updatePointerTarget();

    MWWorld::Ptr getTarget(const std::string& directorNode);

    osg::Matrix getWeaponTransformMatrix() const;

protected:
    static osg::ref_ptr<osg::Geometry> createPointerGeometry(void);

    float getVelocity(const std::string& groupname) const override;

public:
    std::shared_ptr<VRSession> mSession;
    osg::ref_ptr<ForearmController> mForearmControllers[2];
    osg::ref_ptr<HandController> mHandControllers[2];
    osg::ref_ptr<FingerController> mIndexFingerControllers[2];
    osg::ref_ptr<osg::MatrixTransform> mModelOffset;

    bool mIsPointingForward{ false };
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
