#include "vranimation.hpp"

#include <osg/UserDataContainer>
#include <osg/MatrixTransform>
#include <osg/PositionAttitudeTransform>
#include <osg/Depth>
#include <osg/Drawable>
#include <osg/Object>
#include <osg/BlendFunc>

#include <osgUtil/RenderBin>
#include <osgUtil/CullVisitor>


#include <components/debug/debuglog.hpp>

#include <components/misc/rng.hpp>

#include <components/misc/resourcehelpers.hpp>

#include <components/resource/resourcesystem.hpp>
#include <components/resource/scenemanager.hpp>
#include <components/sceneutil/actorutil.hpp>
#include <components/sceneutil/attach.hpp>
#include <components/sceneutil/clone.hpp>
#include <components/sceneutil/visitor.hpp>
#include <components/sceneutil/skeleton.hpp>
#include <components/sceneutil/riggeometry.hpp>
#include <components/sceneutil/positionattitudetransform.hpp>

#include <components/settings/settings.hpp>

#include <components/nifosg/nifloader.hpp> // TextKeyMapHolder

#include <components/vfs/manager.hpp>

#include "../mwworld/esmstore.hpp"
#include "../mwworld/inventorystore.hpp"
#include "../mwworld/class.hpp"
#include "../mwworld/player.hpp"

#include "../mwmechanics/npcstats.hpp"
#include "../mwmechanics/actorutil.hpp"
#include "../mwmechanics/weapontype.hpp"

#include "../mwbase/environment.hpp"
#include "../mwbase/world.hpp"
#include "../mwbase/mechanicsmanager.hpp"
#include "../mwbase/soundmanager.hpp"
#include "../mwbase/windowmanager.hpp"

#include "../mwrender/camera.hpp"
#include "../mwrender/rotatecontroller.hpp"
#include "../mwrender/renderbin.hpp"
#include "../mwrender/vismask.hpp"
#include "../mwrender/renderingmanager.hpp"
#include "../mwrender/objects.hpp"

#include "../mwphysics/collisiontype.hpp"
#include "../mwphysics/physicssystem.hpp"

#include "vrenvironment.hpp"
#include "openxrviewer.hpp"
#include "openxrinputmanager.hpp"

namespace MWVR
{

// This will work for a prototype. But finger/arm control might be better implemented using the 
// existing animation system, implementing this as an animation source.
// But I'm not sure it would be since these are not classical animations.
// It would make it easier to control priority, and later allow for users to add their own stuff to animations based on VR/touch input.
// But openmw doesn't really have any concepts for user animation overrides as far as i can tell.


/// Implements dummy control of the forearm, to control mesh/bone deformation of the hand.
class ForearmController : public osg::NodeCallback
{
public:
    ForearmController(osg::Node* relativeTo, SceneUtil::PositionAttitudeTransform* tracker);
    void setEnabled(bool enabled) { mEnabled = enabled; };
    void operator()(osg::Node* node, osg::NodeVisitor* nv);

private:
    bool mEnabled = true;
    osg::Quat mRotate{};
    osg::Node* mRelativeTo;
    osg::Matrix mOffset{ osg::Matrix::identity() };
    bool mOffsetInitialized = false;
    SceneUtil::PositionAttitudeTransform* mTracker;
};

ForearmController::ForearmController(osg::Node* relativeTo, SceneUtil::PositionAttitudeTransform* tracker)
    : mRelativeTo(relativeTo)
    , mTracker(tracker)
{
}

void ForearmController::operator()(osg::Node* node, osg::NodeVisitor* nv)
{
    if (!mEnabled)
    {
        traverse(node, nv);
        return;
    }

    osg::MatrixTransform* transform = static_cast<osg::MatrixTransform*>(node);
    if (!mOffsetInitialized)
    {
        // This is a bit of a hack.
        // Trackers track hands, not forearms.
        // But i have to transform the forearms to account for deformations,
        // so i subtract the hand transform from the final transform to center the hands.
        std::string handName = node->getName() == "Bip01 L Forearm" ? "Bip01 L Hand" : "Bip01 R Hand";
        SceneUtil::FindByNameVisitor findHandVisitor(handName);
        node->accept(findHandVisitor);
        mOffset = findHandVisitor.mFoundNode->asTransform()->asMatrixTransform()->getInverseMatrix();
        mOffsetInitialized = true;
    }
    // Get current world transform of limb
    osg::Matrix worldToLimb = osg::computeLocalToWorld(node->getParentalNodePaths()[0]);
    // Get current world of the reference node
    osg::Matrix worldReference = osg::Matrix::identity();
    // New transform is reference node + tracker.
    mTracker->computeLocalToWorldMatrix(worldReference, nullptr);
    // Get hand
    transform->setMatrix(mOffset * worldReference * osg::Matrix::inverse(worldToLimb) * transform->getMatrix());


    // TODO: Continued traversal is necessary to allow update of new hand poses such as gripping a weapon.
    // But I want to disable idle animations.
    traverse(node, nv);
}

/// Implements control of a finger by overriding rotation
class FingerController : public osg::NodeCallback
{
public:
    FingerController(osg::Quat rotate) : mRotate(rotate) {};
    void setEnabled(bool enabled) { mEnabled = enabled; };
    void operator()(osg::Node* node, osg::NodeVisitor* nv);

private:
    bool mEnabled = true;
    osg::Quat mRotate{};
};

void FingerController::operator()(osg::Node* node, osg::NodeVisitor* nv)
{
    if (!mEnabled)
    {
        traverse(node, nv);
        return;
    }

    // This update needs to hard override all other animation updates.
    // To do this i need to make sure no further update calls are made.
    // Therefore i do not traverse normally but instead explicitly fetch 
    // the children i want to update and update them here.

    // I'm sure this could be done in a cleaner way


    // First, update the base of the finger to the overriding orientation
    auto matrixTransform = node->asTransform()->asMatrixTransform();
    auto matrix = matrixTransform->getMatrix();
    matrix.setRotate(mRotate);
    matrixTransform->setMatrix(matrix);

    // Next update the tip.
    // Note that for now both tips are just given osg::Quat(0,0,0,1) as that amounts to pointing forward.
    auto tip = matrixTransform->getChild(0)->asTransform()->asMatrixTransform();
    matrix = tip->getMatrix();
    matrix.setRotate(mRotate);
    tip->setMatrix(matrix);

    // Finally, if pointing forward is enabled we need to intersect the scene to find where the player is pointing
    // So that we can display a beam to visualize where the player is pointing.

    // Dig up the pointer transform
    auto* anim = MWVR::Environment::get().getPlayerAnimation();
    if (anim)
        anim->updatePointerTarget();
}


VRAnimation::VRAnimation(
    const MWWorld::Ptr& ptr, osg::ref_ptr<osg::Group> parentNode, Resource::ResourceSystem* resourceSystem,
    bool disableSounds, std::shared_ptr<OpenXRSession> xrSession)
    // Note that i let it construct as 3rd person and then later update it to VM_VRHeadless
    // when OpenMW sets the view mode of the camera object.
    : MWRender::NpcAnimation(ptr, parentNode, resourceSystem, disableSounds, VM_Normal, 55.f)
    , mSession(xrSession)
    , mIndexFingerControllers{nullptr, nullptr}
    // The player model needs to be pushed back a little to make sure the player's view point is naturally protruding 
    // Pushing the camera forward instead would produce an unnatural extra movement when rotating the player model.
    , mModelOffset(new osg::MatrixTransform(osg::Matrix::translate(osg::Vec3(0,-15,0))))
{
    mIndexFingerControllers[0] = osg::ref_ptr<FingerController> (new FingerController(osg::Quat(0, 0, 0, 1)));
    mIndexFingerControllers[1] = osg::ref_ptr<FingerController> (new FingerController(osg::Quat(0, 0, 0, 1)));
    mModelOffset->setName("ModelOffset");
    createPointer();
}

VRAnimation::~VRAnimation() {};

void VRAnimation::setViewMode(NpcAnimation::ViewMode viewMode)
{
    if (viewMode != VM_VRHeadless)
        Log(Debug::Warning) << "View mode of VRAnimation may only be VM_VRHeadless";
    NpcAnimation::setViewMode(VM_VRHeadless);
    return;
}

void VRAnimation::updateParts()
{
    NpcAnimation::updateParts();

    // Hide head and hair to avoid getting them in the player's face
    // TODO: Hair might be acceptable ?
    removeIndividualPart(ESM::PartReferenceType::PRT_Hair);
    removeIndividualPart(ESM::PartReferenceType::PRT_Head);
    removeIndividualPart(ESM::PartReferenceType::PRT_LForearm);
    removeIndividualPart(ESM::PartReferenceType::PRT_LUpperarm);
    removeIndividualPart(ESM::PartReferenceType::PRT_LWrist);
    removeIndividualPart(ESM::PartReferenceType::PRT_RForearm);
    removeIndividualPart(ESM::PartReferenceType::PRT_RUpperarm);
    removeIndividualPart(ESM::PartReferenceType::PRT_RWrist);
    removeIndividualPart(ESM::PartReferenceType::PRT_Cuirass);
    removeIndividualPart(ESM::PartReferenceType::PRT_Groin);
    removeIndividualPart(ESM::PartReferenceType::PRT_Neck);
    removeIndividualPart(ESM::PartReferenceType::PRT_Skirt);
    removeIndividualPart(ESM::PartReferenceType::PRT_Tail);
    removeIndividualPart(ESM::PartReferenceType::PRT_LLeg);
    removeIndividualPart(ESM::PartReferenceType::PRT_RLeg);
    removeIndividualPart(ESM::PartReferenceType::PRT_LAnkle);
    removeIndividualPart(ESM::PartReferenceType::PRT_RAnkle);
}
void VRAnimation::setPointForward(bool enabled)
{
    auto found00 = mNodeMap.find("bip01 r finger1");
    //auto weapon = mNodeMap.find("weapon");
    if (found00 != mNodeMap.end())
    {
        auto base_joint = found00->second;
        auto second_joint = base_joint->getChild(0)->asTransform()->asMatrixTransform();
        assert(second_joint);

        second_joint->removeChild(mPointerTransform);
        //weapon->second->removeChild(mWeaponDirectionTransform);
        mWeaponDirectionTransform->removeChild(mPointerGeometry);
        base_joint->removeUpdateCallback(mIndexFingerControllers[0]);
        if (enabled)
        {
            second_joint->addChild(mPointerTransform);
            //weapon->second->addChild(mWeaponDirectionTransform);
            mWeaponDirectionTransform->addChild(mPointerGeometry);
            base_joint->addUpdateCallback(mIndexFingerControllers[0]);
        }
    }
}

void VRAnimation::createPointer(void)
{
    mPointerGeometry = createPointerGeometry();
    mPointerTransform = new osg::MatrixTransform();
    mPointerTransform->addChild(mPointerGeometry);
    mPointerTransform->setName("Pointer Transform");

    mWeaponPointerTransform = new osg::MatrixTransform();
    mWeaponPointerTransform->addChild(mPointerGeometry);
    mWeaponPointerTransform->setMatrix(
        osg::Matrix::scale(64.f, 1.f, 1.f)
    );
    mWeaponPointerTransform->setName("Weapon Pointer");

    mWeaponDirectionTransform = new osg::MatrixTransform();
    mWeaponDirectionTransform->addChild(mWeaponPointerTransform);
    mWeaponDirectionTransform->setMatrix(
        osg::Matrix::rotate(osg::DegreesToRadians(-90.f), osg::Y_AXIS)
    );
    mWeaponDirectionTransform->setName("Weapon Direction");

    mWeaponAdjustment = new osg::MatrixTransform();
    //mWeaponAdjustment->addChild(mWeaponPointerTransform);
    mWeaponAdjustment->setMatrix(
        osg::Matrix::rotate(osg::DegreesToRadians(90.f), osg::Y_AXIS)
    );
    mWeaponAdjustment->setName("Weapon Adjustment");

}

osg::ref_ptr<osg::Geometry> VRAnimation::createPointerGeometry(void)
{
    osg::ref_ptr<osg::Geometry> geometry = new osg::Geometry();

    // Create pointer geometry, which will point from the tip of the player's finger.
    // The geometry will be a Four sided pyramid, with the top at the player's fingers

    osg::Vec3 vertices[]{
        {0, 0, 0}, // origin
        {1, 1, -1}, // top_left
        {1, -1, -1}, // bottom_left
        {1, -1, 1}, // bottom_right
        {1, 1, 1}, // top_right
    };

    osg::Vec4 colors[]{
        osg::Vec4(1.0f, 0.0f, 0.0f, 0.0f),
        osg::Vec4(1.0f, 0.0f, 0.0f, 1.0f),
        osg::Vec4(1.0f, 0.0f, 0.0f, 1.0f),
        osg::Vec4(1.0f, 0.0f, 0.0f, 1.0f),
        osg::Vec4(1.0f, 0.0f, 0.0f, 1.0f),
    };

    const int origin = 0;
    const int top_left = 1;
    const int bottom_left = 2;
    const int bottom_right = 3;
    const int top_right = 4;

    const int triangles[] =
    {
        bottom_right, top_right, top_left,
        bottom_right, top_left, bottom_left,
        origin, top_left, top_right,
        origin, top_right, bottom_right,
        origin, bottom_left, top_left,
        origin, bottom_right, bottom_left,
    };
    int numVertices = sizeof(triangles) / sizeof(*triangles);
    osg::ref_ptr<osg::Vec3Array> vertexArray = new osg::Vec3Array(numVertices);
    osg::ref_ptr<osg::Vec4Array> colorArray = new osg::Vec4Array(numVertices);
    for (int i = 0; i < numVertices; i++)
    {
        (*vertexArray)[i] = vertices[triangles[i]];
        (*colorArray)[i] = colors[triangles[i]];
    }

    osg::ref_ptr<osg::Vec3Array> normals = new osg::Vec3Array;
    normals->push_back(osg::Vec3(0.0f, -1.0f, 0.0f));



    geometry->setVertexArray(vertexArray);
    geometry->setColorArray(colorArray, osg::Array::BIND_PER_VERTEX);
    geometry->addPrimitiveSet(new osg::DrawArrays(osg::PrimitiveSet::TRIANGLES, 0, numVertices));
    geometry->setDataVariance(osg::Object::DYNAMIC);
    geometry->setSupportsDisplayList(false);
    geometry->setCullingActive(false);

    auto stateset = geometry->getOrCreateStateSet();
    stateset->setMode(GL_LIGHTING, osg::StateAttribute::OFF);
    stateset->setMode(GL_BLEND, osg::StateAttribute::ON);
    stateset->setAttributeAndModes(new osg::BlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA));
    stateset->setRenderingHint(osg::StateSet::TRANSPARENT_BIN);

    return geometry;
}

osg::Vec3f VRAnimation::runAnimation(float timepassed)
{
    return NpcAnimation::runAnimation(timepassed);
}

void VRAnimation::addControllers()
{
    NpcAnimation::addControllers();

    // TODO: 
    // Those controllers should be made using the openxr session *here* rather than magicking up
    // a couple nodes by searching the scene graph.

    //mNodeMap, mActiveControllers, mObjectRoot.get()
    SceneUtil::FindByNameVisitor findXRVisitor("OpenXRRoot", osg::NodeVisitor::TRAVERSE_PARENTS);
    getObjectRoot()->accept(findXRVisitor);
    auto* xrRoot = findXRVisitor.mFoundNode;
    if (!xrRoot)
    {
        throw std::logic_error("Viewmode is VM_VRHeadless but OpenXRRoot does not exist");
    }

    for (int i = 0; i < 2; ++i)
    {
        mHandControllers[i] = nullptr;

        SceneUtil::FindByNameVisitor findTrackerVisitor(i == 0 ? "tracker l hand" : "tracker r hand");
        xrRoot->accept(findTrackerVisitor);
        if (!findTrackerVisitor.mFoundNode)
            continue;

        SceneUtil::PositionAttitudeTransform* tracker = dynamic_cast<SceneUtil::PositionAttitudeTransform*>(findTrackerVisitor.mFoundNode);

        auto found = mNodeMap.find(i == 0 ? "bip01 l forearm" : "bip01 r forearm");
        if (found != mNodeMap.end())
        {
            osg::Node* node = found->second;
            mForearmControllers[i] = new ForearmController(mObjectRoot, tracker);
            node->addUpdateCallback(mForearmControllers[i]);
            mActiveControllers.insert(std::make_pair(node, mForearmControllers[i]));
        }
    }


    auto parent = mObjectRoot->getParent(0);

    if (parent->getName() == "Player Root")
    {
        auto group = parent->asGroup();
        group->removeChildren(0, parent->getNumChildren());
        group->addChild(mModelOffset);
        mModelOffset->addChild(mObjectRoot);

        auto weapon = mNodeMap.find("weapon");
        weapon->second->addChild(mWeaponDirectionTransform);
    }



    

}
void VRAnimation::enableHeadAnimation(bool)
{
    NpcAnimation::enableHeadAnimation(false);
}
void VRAnimation::setAccurateAiming(bool)
{
    NpcAnimation::setAccurateAiming(false);
}

bool VRAnimation::canPlaceObject()
{
    const float maxDist = 200.f;
    if (mPointerTarget.mHit)
    {
        // check if the wanted position is on a flat surface, and not e.g. against a vertical wall
        if (std::acos((mPointerTarget.mHitNormalWorld / mPointerTarget.mHitNormalWorld.length()) * osg::Vec3f(0, 0, 1)) >= osg::DegreesToRadians(30.f))
            return false;

        return true;
    }

    return false;
}

const MWRender::RayResult& VRAnimation::getPointerTarget() const
{
    return mPointerTarget;
}

void VRAnimation::updatePointerTarget()
{
    auto* world = MWBase::Environment::get().getWorld();
    if (world)
    {
        mPointerTransform->setMatrix(osg::Matrix::scale(1, 1, 1));
        mDistanceToPointerTarget = world->getTargetObject(mPointerTarget, mPointerTransform);

        if(mDistanceToPointerTarget >= 0)
            mPointerTransform->setMatrix(osg::Matrix::scale(mDistanceToPointerTarget, 0.25f, 0.25f));
        else
            mPointerTransform->setMatrix(osg::Matrix::scale(10000.f, 0.25f, 0.25f));
    }
}

}
