#ifndef MWVR_OPENRXANIMATION_H
#define MWVR_OPENRXANIMATION_H

#include "../mwrender/npcanimation.hpp"
#include "openxrmanager.hpp"
#include "openxrsession.hpp"

namespace MWVR
{

class HandController;
class FingerController;
class ForearmController;

/// Subclassing NpcAnimation to override behaviours not compatible with VR
class OpenXRAnimation : public MWRender::NpcAnimation
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
    OpenXRAnimation(const MWWorld::Ptr& ptr, osg::ref_ptr<osg::Group> parentNode, Resource::ResourceSystem* resourceSystem,
                 bool disableSounds, std::shared_ptr<OpenXRSession> xrSession );
    virtual ~OpenXRAnimation();

    /// Overridden to always be false
    virtual void enableHeadAnimation(bool enable);

    /// Overridden to always be false
    virtual void setAccurateAiming(bool enabled);

    /// Overridden, implementation tbd
    virtual osg::Vec3f runAnimation(float timepassed);

    /// A relative factor (0-1) that decides if and how much the skeleton should be pitched
    /// to indicate the facing orientation of the character.
    virtual void setPitchFactor(float factor) { mPitchFactor = factor; }

    /// Overriden to always be VM_VRHeadless
    virtual void setViewMode(ViewMode viewMode);

    /// Overriden to include VR modifications
    virtual void updateParts();

    /// Overrides finger animations to point forward
    /// (Used to visualize direction of activation action)
    void setPointForward(bool enabled);

public:
    void createPointer(void);
    static osg::ref_ptr<osg::Geometry> createPointerGeometry(void);

private:
    std::shared_ptr<OpenXRSession> mSession;
    ForearmController* mForearmControllers[2]{};
    HandController* mHandControllers[2]{};
    osg::ref_ptr<FingerController> mIndexFingerControllers[2];
    osg::ref_ptr<osg::MatrixTransform> mModelOffset;
    osg::ref_ptr<osg::Geometry> mPointerGeometry{ nullptr };
    osg::ref_ptr<osg::Transform> mPointerTransform{ nullptr };
    
};

}

#endif
