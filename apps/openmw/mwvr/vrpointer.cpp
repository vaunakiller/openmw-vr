#include "vrpointer.hpp"
#include "vrutil.hpp"
#include "vrgui.hpp"

#include <osg/MatrixTransform>
#include <osg/Drawable>
#include <osg/BlendFunc>
#include <osg/Fog>
#include <osg/LightModel>
#include <osg/Material>

#include <components/debug/debuglog.hpp>
#include <components/resource/resourcesystem.hpp>
#include <components/resource/scenemanager.hpp>

#include <components/sceneutil/shadow.hpp>
#include <components/sceneutil/positionattitudetransform.hpp>

#include <components/vr/trackingmanager.hpp>
#include <components/vr/viewer.hpp>

#include "../mwbase/environment.hpp"
#include "../mwbase/world.hpp"
#include "../mwbase/windowmanager.hpp"
#include "../mwbase/soundmanager.hpp"

#include "../mwrender/renderingmanager.hpp"
#include "../mwrender/vismask.hpp"

#include "../mwgui/itemmodel.hpp"
#include "../mwgui/draganddrop.hpp"
#include "../mwgui/inventorywindow.hpp"

#include "../mwmechanics/actorutil.hpp"
#include "../mwmechanics/security.hpp"

#include "../mwworld/player.hpp"
#include "../mwworld/class.hpp"

namespace MWVR
{
    /**
     * Makes it possible to use ItemModel::moveItem to move an item from an inventory to the world.
     */
    class DropItemAtPointModel : public MWGui::ItemModel
    {
    public:
        DropItemAtPointModel(UserPointer* pointer) : mVRPointer(pointer) {}
        ~DropItemAtPointModel() {}
        MWWorld::Ptr copyItem(const MWGui::ItemStack& item, size_t count, bool /*allowAutoEquip*/) override
        {
            MWBase::World* world = MWBase::Environment::get().getWorld();

            MWWorld::Ptr dropped;
            if (mVRPointer->canPlaceObject())
                dropped = world->placeObject(item.mBase, mVRPointer->getPointerRay(), count);
            else
                dropped = world->dropObjectOnGround(world->getPlayerPtr(), item.mBase, count);
            dropped.getCellRef().setOwner("");

            return dropped;
        }

        void removeItem(const MWGui::ItemStack& item, size_t count) override { throw std::runtime_error("removeItem not implemented"); }
        ModelIndex getIndex(const MWGui::ItemStack& item) override { throw std::runtime_error("getIndex not implemented"); }
        void update() override {}
        size_t getItemCount() override { return 0; }
        MWGui::ItemStack getItem(ModelIndex index) override { throw std::runtime_error("getItem not implemented"); }
        bool usesContainer(const MWWorld::Ptr&) override { return false; }

    private:
        // Where to drop the item
        MWRender::RayResult mIntersection;
        UserPointer* mVRPointer;
    };

    UserPointer::UserPointer(osg::Group* root)
        : mRoot(root)
    {
        mPointerGeometry = createPointerGeometry();
        mPointerTransform = new SceneUtil::PositionAttitudeTransform();
        mPointerTransform->addChild(mPointerGeometry);
        mPointerTransform->setNodeMask(MWRender::VisMask::Mask_Pointer);
        mPointerTransform->setName("VR Pointer deformation");
        //mPointerSource = new SceneUtil::PositionAttitudeTransform();
        //mPointerSource->addChild(mPointerTransform);
        //mPointerSource->setNodeMask(MWRender::VisMask::Mask_Pointer);
        //mPointerSource->setName("VR Pointer source transform");
    }

    UserPointer::~UserPointer()
    {
    }

    void UserPointer::setSource(VR::VRPath source)
    {
        if (mSourcePath == source)
            return;

        mSourcePath = source;
        
    }

    void UserPointer::activate()
    {
        if (mPointerRay.mHit)
        {
            MWWorld::Ptr target = mPointerRay.mHitObject;
            auto wm = MWBase::Environment::get().getWindowManager();
            auto& dnd = wm->getDragAndDrop();
            if (dnd.mIsOnDragAndDrop)
            {
                // Intersected with the world while drag and drop is active
                // Drop item into the world
                MWBase::Environment::get().getWorld()->breakInvisibility(
                    MWMechanics::getPlayer());
                DropItemAtPointModel drop(this);
                dnd.drop(&drop, nullptr);
            }
            else if (!target.isEmpty())
            {
                if (wm->isConsoleMode())
                    wm->setConsoleSelectedObject(target);
                // Don't active things during GUI mode.
                else if (wm->isGuiMode())
                {
                    if (wm->getMode() != MWGui::GM_Container && wm->getMode() != MWGui::GM_Inventory)
                        return;
                    wm->getInventoryWindow()->pickUpObject(target);
                }
                else
                {
                    MWWorld::Player& player = MWBase::Environment::get().getWorld()->getPlayer();
                    if (!tryProbePick(target))
                        player.activate(target);
                }
            }
        }
    }

    bool UserPointer::tryProbePick(MWWorld::Ptr target)
    {
        std::string resultMessage = "";
        std::string resultSound = "";

        MWWorld::Player& player = MWBase::Environment::get().getWorld()->getPlayer();
        if (player.getDrawState() == MWMechanics::DrawState::Weapon)
        {
            MWWorld::Ptr playerPtr = player.getPlayer();
            MWWorld::ContainerStoreIterator rightHandItem = playerPtr.getClass().getInventoryStore(playerPtr).getSlot(MWWorld::InventoryStore::Slot_CarriedRight);
            MWWorld::Ptr tool = *rightHandItem;

            if (!target.isEmpty() && (target.getCellRef().getLockLevel() != 0))
            {
                if (tool.getType() == ESM::Lockpick::sRecordId)
                    MWMechanics::Security(playerPtr).pickLock(target, tool, resultMessage, resultSound);
                else if (tool.getType() == ESM::Probe::sRecordId)
                    MWMechanics::Security(playerPtr).probeTrap(target, tool, resultMessage, resultSound);
            }

            if (!resultMessage.empty())
                MWBase::Environment::get().getWindowManager()->messageBox(resultMessage);
            if (!resultSound.empty())
            {
                MWBase::SoundManager* sndMgr = MWBase::Environment::get().getSoundManager();
                sndMgr->playSound3D(target, resultSound, 1.0f, 1.0f);
            }
        }

        return !(resultSound.empty() && resultMessage.empty());
    }

    const MWRender::RayResult& UserPointer::getPointerRay() const
    {
        return mPointerRay;
    }

    bool UserPointer::canPlaceObject() const
    {
        return mCanPlaceObject;
    }

    void UserPointer::onTrackingUpdated(VR::TrackingManager& manager, VR::DisplayTime predictedDisplayTime)
    {
        mPointerTransform->setScale(osg::Vec3f(1, 1, 1));
        mPointerTransform->setPosition(osg::Vec3f(0, 0, 0));
        mPointerTransform->setAttitude(osg::Quat(0, 0, 0, 1));

        auto tp = manager.locate(mSourcePath, predictedDisplayTime);

        if (!tp.status)
        {
            mRoot->removeChild(mPointerTransform);
            MWVR::VRGUIManager::instance().updateFocus(nullptr, osg::Vec3(0, 0, 0));
            return;
        }

        //mPointerSource->setPosition(tp.pose.position);
        //mPointerSource->setAttitude(tp.pose.orientation);

        mDistanceToPointerTarget = Util::getPoseTarget(mPointerRay, tp.pose, true);
        // Make a ref-counted copy of the target node to ensure the object's lifetime this frame.
        mPointerTarget = mPointerRay.mHitNode;

        mCanPlaceObject = false;
        if (mPointerRay.mHit && mDistanceToPointerTarget > 0.f)
        {
            // check if the wanted position is on a flat surface, and not e.g. against a vertical wall
            mCanPlaceObject = !(std::acos((mPointerRay.mHitNormalWorld / mPointerRay.mHitNormalWorld.length()) * osg::Vec3f(0, 0, 1)) >= osg::DegreesToRadians(30.f));

            Stereo::Pose pose;
            pose.position = Stereo::Position::fromMWUnits(0.f, 2.f * mDistanceToPointerTarget / 3.f, 0.f);
            pose.orientation = osg::Quat(0, 0, 0, 1);
            pose = tp.pose + pose;

            mPointerTransform->setPosition(pose.position.asMWUnits());
            mPointerTransform->setAttitude(pose.orientation);
            mPointerTransform->setScale(osg::Vec3f(0.25f, mDistanceToPointerTarget / 3.f, 0.25f));
            mRoot->addChild(mPointerTransform);

            MWVR::VRGUIManager::instance().updateFocus(mPointerRay.mHitNode, mPointerRay.mHitPointLocal);
        }
        else
        {
            mRoot->removeChild(mPointerTransform);
            MWVR::VRGUIManager::instance().updateFocus(nullptr, osg::Vec3(0, 0, 0));
        }
    }

    osg::ref_ptr<osg::Geometry> UserPointer::createPointerGeometry()
    {
        osg::ref_ptr<osg::Geometry> geometry = new osg::Geometry();

        // Create pointer geometry, which will point from the tip of the player's finger.
        // The geometry will be a Four sided pyramid, with the top at the player's fingers

        osg::Vec3 vertices[]{
            {0, 0, 0}, // origin
            {-1, 1, -1}, // A
            {-1, 1, 1}, //  B
            {1, 1, 1}, //   C
            {1, 1, -1}, //  D
        };

        osg::Vec4 colors[]{
            osg::Vec4(1.0f, 0.0f, 0.0f, 0.0f),
            osg::Vec4(1.0f, 0.0f, 0.0f, 1.0f),
            osg::Vec4(1.0f, 0.0f, 0.0f, 1.0f),
            osg::Vec4(1.0f, 0.0f, 0.0f, 1.0f),
            osg::Vec4(1.0f, 0.0f, 0.0f, 1.0f),
        };

        const int O = 0;
        const int A = 1;
        const int B = 2;
        const int C = 3;
        const int D = 4;

        const int triangles[] =
        {
            A,D,B,
            B,D,C,
            O,D,A,
            O,C,D,
            O,B,C,
            O,A,B,
        };
        int numVertices = sizeof(triangles) / sizeof(*triangles);
        osg::ref_ptr<osg::Vec3Array> vertexArray = new osg::Vec3Array(numVertices);
        osg::ref_ptr<osg::Vec4Array> colorArray = new osg::Vec4Array(numVertices);
        for (int i = 0; i < numVertices; i++)
        {
            (*vertexArray)[i] = vertices[triangles[i]];
            (*colorArray)[i] = colors[triangles[i]];
        }

        geometry->setVertexArray(vertexArray);
        geometry->setColorArray(colorArray, osg::Array::BIND_PER_VERTEX);
        geometry->addPrimitiveSet(new osg::DrawArrays(osg::PrimitiveSet::TRIANGLES, 0, numVertices));
        geometry->setSupportsDisplayList(false);
        geometry->setDataVariance(osg::Object::STATIC);
        geometry->setName("VRPointer Geometry");

        auto stateset = geometry->getOrCreateStateSet();
        stateset->setMode(GL_LIGHTING, osg::StateAttribute::OFF);
        stateset->setMode(GL_BLEND, osg::StateAttribute::ON);
        stateset->setAttributeAndModes(new osg::BlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA));
        stateset->setRenderingHint(osg::StateSet::TRANSPARENT_BIN);
        osg::ref_ptr<osg::Fog> fog(new osg::Fog);
        fog->setStart(10000000);
        fog->setEnd(10000000);
        stateset->setAttributeAndModes(fog, osg::StateAttribute::OFF | osg::StateAttribute::OVERRIDE);

        osg::ref_ptr<osg::LightModel> lightmodel = new osg::LightModel;
        lightmodel->setAmbientIntensity(osg::Vec4(1.0, 1.0, 1.0, 1.0));
        stateset->setAttributeAndModes(lightmodel, osg::StateAttribute::ON);
        SceneUtil::ShadowManager::disableShadowsForStateSet(stateset);

        osg::ref_ptr<osg::Material> material = new osg::Material;
        material->setColorMode(osg::Material::ColorMode::AMBIENT_AND_DIFFUSE);
        stateset->setAttributeAndModes(material, osg::StateAttribute::ON);

        MWBase::Environment::get().getResourceSystem()->getSceneManager()->recreateShaders(geometry, "objects", true);

        return geometry;
    }

}
