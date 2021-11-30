#include "vrutil.hpp"
#include "vranimation.hpp"
#include "vrgui.hpp"
#include "vrpointer.hpp"
#include "vrinputmanager.hpp"

#include "../mwbase/environment.hpp"
#include "../mwbase/windowmanager.hpp"
#include "../mwbase/world.hpp"
#include "../mwmechanics/actorutil.hpp"

#include "../mwworld/class.hpp"
#include "../mwrender/renderingmanager.hpp"

#include <components/vr/trackingmanager.hpp>

#include "osg/Transform"

namespace MWVR
{
    namespace Util
    {
        std::pair<MWWorld::Ptr, float> getPointerTarget()
        {
            auto pointer = MWVR::VRGUIManager::instance().getUserPointer();
            return std::pair<MWWorld::Ptr, float>(pointer->getPointerTarget().mHitObject, pointer->distanceToPointerTarget());
        }

        std::pair<MWWorld::Ptr, float> getTouchTarget()
        {
            MWRender::RayResult result;
            auto rightHandPath = VR::stringToVRPath("/world/user/hand/right/input/aim/pose");
            auto pose = VR::TrackingManager::instance().locate(rightHandPath, 0).pose;
            auto distance = getPoseTarget(result, pose, true);
            return std::pair<MWWorld::Ptr, float>(result.mHitObject, distance);
        }

        std::pair<MWWorld::Ptr, float> getWeaponTarget()
        {
            auto ptr = MWBase::Environment::get().getWorld()->getPlayerPtr();
            auto* anim = MWBase::Environment::get().getWorld()->getAnimation(ptr);

            MWRender::RayResult result;
            auto distance = getPoseTarget(result, getNodePose(anim->getNode("weapon bone")), false);
            return std::pair<MWWorld::Ptr, float>(result.mHitObject, distance);
        }

        float getPoseTarget(MWRender::RayResult& result, const Stereo::Pose& pose, bool allowTelekinesis)
        {
            auto* wm = MWBase::Environment::get().getWindowManager();
            auto* world = MWBase::Environment::get().getWorld();

            if (wm->isGuiMode() && wm->isConsoleMode())
                return world->getTargetObject(result, pose.position, pose.orientation, world->getMaxActivationDistance() * 50, true);
            else
            {
                float activationDistance = 0.f;
                if (allowTelekinesis)
                    activationDistance = world->getActivationDistancePlusTelekinesis();
                else
                    activationDistance = world->getMaxActivationDistance();

                auto distance = world->getTargetObject(result, pose.position, pose.orientation, activationDistance, true);

                if (!result.mHitObject.isEmpty() && !result.mHitObject.getClass().allowTelekinesis(result.mHitObject)
                    && distance > activationDistance && !MWBase::Environment::get().getWindowManager()->isGuiMode())
                {
                    result.mHit = false;
                    result.mHitObject = nullptr;
                    distance = 0.f;
                };
                return distance;
            }
        }

        Stereo::Pose getWeaponPose()
        {
            auto ptr = MWBase::Environment::get().getWorld()->getPlayerPtr();
            auto* anim = MWBase::Environment::get().getWorld()->getAnimation(ptr);
            auto* vrAnim = static_cast<MWVR::VRAnimation*>(anim);
            osg::Matrix worldMatrix = vrAnim->getWeaponTransformMatrix();
            Stereo::Pose pose;
            pose.position = worldMatrix.getTrans();
            pose.orientation = worldMatrix.getRotate();
            return pose;
        }

        Stereo::Pose getNodePose(const osg::Node* node)
        {
            osg::Matrix worldMatrix = osg::computeLocalToWorld(node->getParentalNodePaths()[0]);
            Stereo::Pose pose;
            pose.position = worldMatrix.getTrans();
            pose.orientation = worldMatrix.getRotate();
            return pose;
        }
        void requestRecenter(bool resetZ)
        {
            auto* inputManager = MWBase::Environment::get().getInputManager();
            assert(inputManager);
            auto vrInputManager = dynamic_cast<MWVR::VRInputManager*>(inputManager);
            assert(vrInputManager);
            vrInputManager->requestRecenter(resetZ);
        }
    }
}
