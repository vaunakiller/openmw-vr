#include "vranimation.hpp"

#include "vrenvironment.hpp"
#include "vrviewer.hpp"
#include "vrinputmanager.hpp"

#include <osg/MatrixTransform>
#include <osg/PositionAttitudeTransform>
#include <osg/Depth>
#include <osg/Drawable>
#include <osg/Object>
#include <osg/BlendFunc>

#include <components/debug/debuglog.hpp>

#include <components/sceneutil/actorutil.hpp>
#include <components/sceneutil/positionattitudetransform.hpp>

#include <components/settings/settings.hpp>

#include "../mwworld/esmstore.hpp"

#include "../mwmechanics/npcstats.hpp"
#include "../mwmechanics/actorutil.hpp"
#include "../mwmechanics/weapontype.hpp"

#include "../mwbase/environment.hpp"
#include "../mwbase/world.hpp"
#include "../mwbase/windowmanager.hpp"

#include "../mwrender/camera.hpp"
#include "../mwrender/renderingmanager.hpp"

namespace MWVR
{

// Some weapon types, such as spellcast, are classified as melee even though they are not.
// All the fake melee types have negative type enum, but also so does hand to hand.
// I think this covers all the cases
static bool isMeleeWeapon(int type)
{
    if (MWMechanics::getWeaponType(type)->mWeaponClass != ESM::WeaponType::Melee)
        return false;
    if (type == ESM::Weapon::HandToHand)
        return true;
    if (type >= 0)
        return true;

    return false;
}

/// Implements VR control of the forearm, to control mesh/bone deformation of the hand.
class ForearmController : public osg::NodeCallback
{
public:
    ForearmController() = default;
    void setEnabled(bool enabled) { mEnabled = enabled; };
    void operator()(osg::Node* node, osg::NodeVisitor* nv);

private:
    bool mEnabled{ true };
};

void ForearmController::operator()(osg::Node* node, osg::NodeVisitor* nv)
{
    if (!mEnabled)
    {
        traverse(node, nv);
        return;
    }

    osg::MatrixTransform* transform = static_cast<osg::MatrixTransform*>(node);

    auto* session = Environment::get().getSession();
    auto* xrViewer = Environment::get().getViewer();
    int side = (int)Side::RIGHT_SIDE;
    if (node->getName().find_first_of("L") != std::string::npos)
    {
        side = (int)Side::LEFT_SIDE;
        // We base ourselves on the world position of the camera
        // Ensure it is updated before placing the hands
        // I'm sure this can be achieved properly by managing the scene graph better.
        MWBase::Environment::get().getWorld()->getRenderingManager().getCamera()->updateCamera();
    }

    MWVR::Pose handStage = session->predictedPoses(VRSession::FramePhase::Update).hands[side];
    MWVR::Pose headStage = session->predictedPoses(VRSession::FramePhase::Update).head;

    auto orientation = handStage.orientation;

    auto position = handStage.position - headStage.position;
    position = position * Environment::get().unitsPerMeter();

    auto camera = xrViewer->mViewer->getCamera();
    auto viewMatrix = camera->getViewMatrix();


    // Align orientation with the game world
    auto* inputManager = Environment::get().getInputManager();
    if (inputManager)
    {
        auto stageRotation = inputManager->stageRotation();
        position = stageRotation * position;
        orientation = orientation * stageRotation;
    }

    // Add camera offset
    osg::Vec3 viewPosition;
    osg::Vec3 center;
    osg::Vec3 up;

    viewMatrix.getLookAt(viewPosition, center, up, 1.0);
    position += viewPosition;

    //// Morrowind's meshes do not point forward by default.
    //// Declare the offsets static since they do not need to be recomputed.
    static float VRbias = osg::DegreesToRadians(-90.f);
    static osg::Quat yaw(-VRbias, osg::Vec3f(0, 0, 1));
    static osg::Quat pitch(2.f * VRbias, osg::Vec3f(0, 1, 0));
    static osg::Quat roll(2 * VRbias, osg::Vec3f(1, 0, 0));
    orientation = yaw * orientation;
    if (side == (int)Side::LEFT_SIDE)
        orientation = roll * orientation;

    // Undo the wrist translate
    auto* hand = transform->getChild(0);
    auto handMatrix = hand->asTransform()->asMatrixTransform()->getMatrix();
    position -= orientation * handMatrix.getTrans();

    // Center hand mesh on tracking
    // This is just an estimate from trial and error, any suggestion for improving this is welcome
    position -= orientation * osg::Vec3{ 15,0,0 };

    // Get current world transform of limb
    osg::Matrix worldToLimb = osg::computeLocalToWorld(node->getParentalNodePaths()[0]);
    // Get current world of the reference node
    osg::Matrix worldReference = osg::Matrix::identity();
    // New transform based on tracking.
    worldReference.preMultTranslate(position);
    worldReference.preMultRotate(orientation);

    // Finally, set transform
    transform->setMatrix(worldReference * osg::Matrix::inverse(worldToLimb) * transform->getMatrix());

    // Omit nested callbacks to override animations of this node
    osg::ref_ptr<osg::Callback> ncb = getNestedCallback();
    setNestedCallback(nullptr);
    traverse(node, nv);
    setNestedCallback(ncb);
}

/// Implements control of a finger by overriding rotation
class FingerController : public osg::NodeCallback
{
public:
    FingerController() {};
    void setEnabled(bool enabled) { mEnabled = enabled; };
    void operator()(osg::Node* node, osg::NodeVisitor* nv);

private:
    bool mEnabled = true;
};

void FingerController::operator()(osg::Node* node, osg::NodeVisitor* nv)
{
    if (!mEnabled)
    {
        traverse(node, nv);
        return;
    }


    osg::Quat rotate{ 0,0,0,1 };

    // TODO: 
    // To make finger pointing more natural each joint should angle down roughly 5-6 degrees per joint.
    // But this creates obvious visual conflicts with the hand and the rest of the fingers which are not posed naturally,
    // and looks particularly riddiculous when a weapon is drawn.
    // Therefore, for the time being i will simply have them point straight forward relative to the current hand rotation.
    // This leads to particularly awkward finger pointing while a weapon is drawn and should be replaced by a 
    // complete override of all hand animations by the end of 2090.

    //////// Add a slight rotation down of the fingers to naturalize the pointing.
    //////rotate = osg::Quat(osg::PI_4 / 8, osg::Vec3{ 0,1,0 }) * rotate;
    //////if (node->getName() == "Bip01 R Finger1")
    //////{
    //////    auto* world = MWBase::Environment::get().getWorld();

    //////    // Morrowind models do not hold weapons at a natural angle, so i rotate the hand forward 
    //////    // to get a more natural angle on the weapon to allow more comfortable combat.
    //////    // Fingers need to angle back up to keep pointing natural.
    //////    if (world->getActiveWeaponType() >= 0)
    //////    {
    //////        rotate = osg::Quat(-osg::PI_4, osg::Vec3{ 0,1,0 }) * rotate;
    //////    }
    //////}

    // First, update the base of the finger to the overriding orientation
    auto matrixTransform = node->asTransform()->asMatrixTransform();
    auto matrix = matrixTransform->getMatrix();
    matrix.setRotate(rotate);
    matrixTransform->setMatrix(matrix);


    // Omit nested callbacks to override animations of this node
    osg::ref_ptr<osg::Callback> ncb = getNestedCallback();
    setNestedCallback(nullptr);
    traverse(node, nv);
    setNestedCallback(ncb);

    // Update where the player is currently pointing
    auto* anim = MWVR::Environment::get().getPlayerAnimation();
    if (anim && node->getName() == "Bip01 R Finger1")
    {
        anim->updatePointerTarget();
    }
}

/// Implements control of a finger by overriding rotation
class HandController : public osg::NodeCallback
{
public:
    HandController() = default;
    void setEnabled(bool enabled) { mEnabled = enabled; };
    void operator()(osg::Node* node, osg::NodeVisitor* nv);

private:
    bool mEnabled = true;
};

void HandController::operator()(osg::Node* node, osg::NodeVisitor* nv)
{
    if (!mEnabled)
    {
        traverse(node, nv);
        return;
    }
    float PI_2 = osg::PI_2;
    if (node->getName() == "Bip01 L Hand")
        PI_2 = -PI_2;
    float PI_4 = PI_2 / 2.f;

    osg::Quat rotate{ 0,0,0,1 };
    auto* world = MWBase::Environment::get().getWorld();
    auto windowManager = MWBase::Environment::get().getWindowManager();
    auto animation = MWVR::Environment::get().getPlayerAnimation();
    auto weaponType = world->getActiveWeaponType();
    // Morrowind models do not hold most weapons at a natural angle, so i rotate the hand
    // to more natural angles on weapons to allow more comfortable combat.
    if (!windowManager->isGuiMode() && !animation->fingerPointingMode())
    {

        switch (weaponType)
        {
        case ESM::Weapon::None:
        case ESM::Weapon::HandToHand:
        case ESM::Weapon::MarksmanThrown:
        case ESM::Weapon::Spell:
        case ESM::Weapon::Arrow:
        case ESM::Weapon::Bolt:
            // No adjustment
            break;
        case ESM::Weapon::MarksmanCrossbow:
            // Crossbow points upwards. Assumedly because i am overriding hand animations.
            rotate = osg::Quat(PI_4 / 1.05, osg::Vec3{ 0,1,0 }) * osg::Quat(0.06, osg::Vec3{ 0,0,1 });
            break;
        case ESM::Weapon::MarksmanBow:
            // Bow points down by default, rotate it back up a little
            rotate = osg::Quat(-PI_2 * .10f, osg::Vec3{ 0,1,0 });
            break;
        default:
            // Melee weapons Need adjustment
            rotate = osg::Quat(PI_4, osg::Vec3{ 0,1,0 });
            break;
        }
    }

    auto matrixTransform = node->asTransform()->asMatrixTransform();
    auto matrix = matrixTransform->getMatrix();
    matrix.setRotate(rotate);
    matrixTransform->setMatrix(matrix);


    // Omit nested callbacks to override animations of this node
    osg::ref_ptr<osg::Callback> ncb = getNestedCallback();
    setNestedCallback(nullptr);
    traverse(node, nv);
    setNestedCallback(ncb);
}

/// Implements control of weapon direction
class WeaponDirectionController : public osg::NodeCallback
{
public:
    WeaponDirectionController() = default;
    void setEnabled(bool enabled) { mEnabled = enabled; };
    void operator()(osg::Node* node, osg::NodeVisitor* nv);

private:
    bool mEnabled = true;
};

void WeaponDirectionController::operator()(osg::Node* node, osg::NodeVisitor* nv)
{
    if (!mEnabled)
    {
        traverse(node, nv);
        return;
    }

    // Arriving here implies a parent, no need to check
    auto parent = static_cast<osg::MatrixTransform*>(node->getParent(0));



    osg::Quat rotate{ 0,0,0,1 };
    auto* world = MWBase::Environment::get().getWorld();
    auto weaponType = world->getActiveWeaponType();
    switch (weaponType)
    {
    case ESM::Weapon::MarksmanThrown:
    case ESM::Weapon::Spell:
    case ESM::Weapon::Arrow:
    case ESM::Weapon::Bolt:
    case ESM::Weapon::HandToHand:
    case ESM::Weapon::MarksmanBow:
    case ESM::Weapon::MarksmanCrossbow:
        // Rotate to point straight forward, reverting any rotation of the hand to keep aim consistent.
        rotate = parent->getInverseMatrix().getRotate();
        rotate = osg::Quat(-osg::PI_2, osg::Vec3{ 0,0,1 }) * rotate;
        break;
    default:
        // Melee weapons point straight up from the hand
        rotate = osg::Quat(osg::PI_2, osg::Vec3{ 1,0,0 });
        break;
    }

    auto matrixTransform = node->asTransform()->asMatrixTransform();
    auto matrix = matrixTransform->getMatrix();
    matrix.setRotate(rotate);
    matrixTransform->setMatrix(matrix);

    traverse(node, nv);
}

/// Implements control of the weapon pointer
class WeaponPointerController : public osg::NodeCallback
{
public:
    WeaponPointerController() = default;
    void setEnabled(bool enabled) { mEnabled = enabled; };
    void operator()(osg::Node* node, osg::NodeVisitor* nv);

private:
    bool mEnabled = true;
};

void WeaponPointerController::operator()(osg::Node* node, osg::NodeVisitor* nv)
{
    if (!mEnabled)
    {
        traverse(node, nv);
        return;
    }

    auto matrixTransform = node->asTransform()->asMatrixTransform();
    auto world = MWBase::Environment::get().getWorld();
    auto weaponType = world->getActiveWeaponType();
    auto windowManager = MWBase::Environment::get().getWindowManager();

    if (!isMeleeWeapon(weaponType) && !windowManager->isGuiMode())
    {
        // Ranged weapons should show a pointer to where they are targeting
        matrixTransform->setMatrix(
            osg::Matrix::scale(1.f, 64.f, 1.f)
        );
    }
    else
    {
        matrixTransform->setMatrix(
            osg::Matrix::scale(1.f, 64.f, 1.f)
        );
    }

    // First, update the base of the finger to the overriding orientation

    traverse(node, nv);
}

VRAnimation::VRAnimation(
    const MWWorld::Ptr& ptr, osg::ref_ptr<osg::Group> parentNode, Resource::ResourceSystem* resourceSystem,
    bool disableSounds, std::shared_ptr<VRSession> xrSession)
    // Note that i let it construct as 3rd person and then later update it to VM_VRHeadless
    // when the character controller updates
    : MWRender::NpcAnimation(ptr, parentNode, resourceSystem, disableSounds, VM_Normal, 55.f)
    , mSession(xrSession)
    , mIndexFingerControllers{nullptr, nullptr}
    // The player model needs to be pushed back a little to make sure the player's view point is naturally protruding 
    // Pushing the camera forward instead would produce an unnatural extra movement when rotating the player model.
    , mModelOffset(new osg::MatrixTransform(osg::Matrix::translate(osg::Vec3(0,-15,0))))
{
    for (int i = 0; i < 2; i++)
    {
        mIndexFingerControllers[i] = new FingerController;
        mForearmControllers[i] = new ForearmController;
        mHandControllers[i] = new HandController;
    }

    mWeaponDirectionTransform = new osg::MatrixTransform();
    mWeaponDirectionTransform->setName("Weapon Direction");
    mWeaponDirectionTransform->setUpdateCallback(new WeaponDirectionController);

    mModelOffset->setName("ModelOffset");
    mPointerGeometry = createPointerGeometry();
    mPointerRescale = new osg::MatrixTransform();
    mPointerRescale->addChild(mPointerGeometry);
    mPointerTransform = new osg::MatrixTransform();
    mPointerTransform->addChild(mPointerRescale);
    mPointerTransform->setName("Pointer Transform");
    // Morrowind's hands don't actually point forward, so we have to reorient the pointer.
    mPointerTransform->setMatrix(osg::Matrix::rotate(osg::Quat(-osg::PI_2, osg::Vec3f(0, 0, 1))));

    mWeaponPointerTransform = new osg::MatrixTransform();
    mWeaponPointerTransform->addChild(mPointerGeometry);
    mWeaponPointerTransform->setMatrix(
        osg::Matrix::scale(0.f, 0.f, 0.f)
    );
    mWeaponPointerTransform->setName("Weapon Pointer");
    mWeaponPointerTransform->setUpdateCallback(new WeaponPointerController);
    //mWeaponDirectionTransform->addChild(mWeaponPointerTransform);
}

VRAnimation::~VRAnimation() {};

void VRAnimation::setViewMode(NpcAnimation::ViewMode viewMode)
{
    if (viewMode != VM_VRFirstPerson && viewMode != VM_VRNormal)
    {
        Log(Debug::Warning) << "Attempted to set view mode of VRAnimation to non-vr mode. Defaulted to VM_VRFirstPerson.";
        viewMode = VM_VRFirstPerson;
    }
    NpcAnimation::setViewMode(viewMode);
    return;
}

void VRAnimation::updateParts()
{
    NpcAnimation::updateParts();

    if (mViewMode == VM_VRFirstPerson)
    {
        // Hide everything other than the hands and feet.
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
        removeIndividualPart(ESM::PartReferenceType::PRT_LKnee);
        removeIndividualPart(ESM::PartReferenceType::PRT_RKnee);
        removeIndividualPart(ESM::PartReferenceType::PRT_LFoot);
        removeIndividualPart(ESM::PartReferenceType::PRT_RFoot);
    }
    else
    {
        removeIndividualPart(ESM::PartReferenceType::PRT_LForearm);
        removeIndividualPart(ESM::PartReferenceType::PRT_LWrist);
        removeIndividualPart(ESM::PartReferenceType::PRT_RForearm);
        removeIndividualPart(ESM::PartReferenceType::PRT_RWrist);
    }


    auto playerPtr = MWMechanics::getPlayer();
    const MWWorld::LiveCellRef<ESM::NPC>* ref = playerPtr.get<ESM::NPC>();
    const ESM::Race* race =
        MWBase::Environment::get().getWorld()->getStore().get<ESM::Race>().find(ref->mBase->mRace);
    bool isMale = ref->mBase->isMale();
    float charHeightFactor = isMale ? race->mData.mHeight.mMale : race->mData.mHeight.mFemale;
    float charHeightBase = 1.8f;
    float charHeight = charHeightBase * charHeightFactor;
    // TODO: Player height should be configurable
    // For now i'm just using my own
    float sizeFactor = 1.85f / charHeight;
    Environment::get().getSession()->setPlayerScale(sizeFactor);
}

void VRAnimation::setFingerPointingMode(bool enabled)
{
    if (enabled == mFingerPointingMode)
        return;

    auto finger = mNodeMap.find("bip01 r finger1");
    if (finger != mNodeMap.end())
    {
        auto base_joint = finger->second;
        auto second_joint = base_joint->getChild(0)->asTransform()->asMatrixTransform();
        assert(second_joint);

        base_joint->removeUpdateCallback(mIndexFingerControllers[0]);
        second_joint->removeUpdateCallback(mIndexFingerControllers[1]);
        if (enabled)
        {
            base_joint->addUpdateCallback(mIndexFingerControllers[0]);
            second_joint->addUpdateCallback(mIndexFingerControllers[1]);
            
        }
    }

    mPointerTransform->removeChild(mPointerRescale);
    if (enabled)
    {
        mPointerTransform->addChild(mPointerRescale);
    }
    else
    {
        mPointerTarget = MWRender::RayResult{};
    }

    mFingerPointingMode = enabled;
}

osg::ref_ptr<osg::Geometry> VRAnimation::createPointerGeometry(void)
{
    osg::ref_ptr<osg::Geometry> geometry = new osg::Geometry();

    // Create pointer geometry, which will point from the tip of the player's finger.
    // The geometry will be a Four sided pyramid, with the top at the player's fingers

    osg::Vec3 vertices[]{
        {0, 0, 0}, // origin
        {1, 1, -1}, // top_left
        {-1, 1, -1}, // bottom_left
        {-1, 1, 1}, // bottom_right
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
    geometry->setSupportsDisplayList(false);
    geometry->setDataVariance(osg::Object::STATIC);

    auto stateset = geometry->getOrCreateStateSet();
    stateset->setMode(GL_LIGHTING, osg::StateAttribute::OFF);
    stateset->setMode(GL_BLEND, osg::StateAttribute::ON);
    stateset->setAttributeAndModes(new osg::BlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA));
    stateset->setRenderingHint(osg::StateSet::TRANSPARENT_BIN);

    return geometry;
}

float VRAnimation::getVelocity(const std::string& groupname) const
{
  return 0.0f;
}

osg::Vec3f VRAnimation::runAnimation(float timepassed)
{
    return NpcAnimation::runAnimation(timepassed);
}

void VRAnimation::addControllers()
{
    NpcAnimation::addControllers();

    for (int i = 0; i < 2; ++i)
    {
        auto forearm = mNodeMap.find(i == 0 ? "bip01 l forearm" : "bip01 r forearm");
        if (forearm != mNodeMap.end())
        {
            auto node = forearm->second;
            node->removeUpdateCallback(mForearmControllers[i]);
            node->addUpdateCallback(mForearmControllers[i]);
        }

        auto hand = mNodeMap.find(i == 0 ? "bip01 l hand" : "bip01 r hand");
        if (hand != mNodeMap.end())
        {
            auto node = hand->second;
            node->removeUpdateCallback(mHandControllers[i]);
            node->addUpdateCallback(mHandControllers[i]);
        }
    }

    auto hand = mNodeMap.find("bip01 r hand");
    if (hand != mNodeMap.end())
    {
        hand->second->removeChild(mWeaponDirectionTransform);
        hand->second->addChild(mWeaponDirectionTransform);
    }
    auto finger = mNodeMap.find("bip01 r finger11");
    if (finger != mNodeMap.end())
    {
        finger->second->removeChild(mPointerTransform);
        finger->second->addChild(mPointerTransform);
    }

    auto parent = mObjectRoot->getParent(0);
    if (parent->getName() == "Player Root")
    {
        auto group = parent->asGroup();
        group->removeChildren(0, parent->getNumChildren());
        group->addChild(mModelOffset);
        mModelOffset->addChild(mObjectRoot);
    }

    //auto wb = mNodeMap.find("weapon bone");
    //if (wb != mNodeMap.end())
    //{
    //    wb->second->removeChild(mWeaponPointerTransform);
    //    wb->second->addChild(mWeaponPointerTransform);
    //}
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


MWWorld::Ptr VRAnimation::getTarget(const std::string& directorNode)
{
    auto node = mNodeMap.find(directorNode);
    auto* world = MWBase::Environment::get().getWorld();
    MWRender::RayResult result{};
    if (node != mNodeMap.end())
        if (world)
            world->getTargetObject(result, node->second);
    return result.mHitObject;
}

osg::Matrix VRAnimation::getWeaponTransformMatrix() const
{
    return osg::computeLocalToWorld(mWeaponDirectionTransform->getParentalNodePaths()[0]);
}

void VRAnimation::updatePointerTarget()
{
    auto* world = MWBase::Environment::get().getWorld();
    if (world)
    {
        mPointerRescale->setMatrix(osg::Matrix::scale(1, 1, 1));
        mDistanceToPointerTarget = world->getTargetObject(mPointerTarget, mPointerTransform);

        if(mDistanceToPointerTarget >= 0)
            mPointerRescale->setMatrix(osg::Matrix::scale(0.25f, mDistanceToPointerTarget, 0.25f));
        else
            mPointerRescale->setMatrix(osg::Matrix::scale(0.25f, 10000.f, 0.25f));
    }
}

}
