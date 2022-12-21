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
            auto* pointer = MWVR::VRInputManager::instance().vrPointer();
            if (pointer)
            {
                const auto& ray = pointer->getPointerRay();
                if(ray.mHit && ray.mHitNode)
                    return std::pair<MWWorld::Ptr, float>(ray.mHitObject, pointer->distanceToPointerTarget());
            }
            return std::pair<MWWorld::Ptr, float>();
        }

        std::pair<MWWorld::Ptr, float> getTouchTarget()
        {
            MWRender::RayResult result;
            std::string pointer = Settings::Manager::getBool("left handed mode", "VR") ?
                "/world/user/hand/left/input/aim/pose"
                : "/world/user/hand/right/input/aim/pose";

            auto handPath = VR::stringToVRPath(pointer);
            auto pose = VR::TrackingManager::instance().locate(handPath, 0).pose;
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
            auto wm = MWBase::Environment::get().getWindowManager();
            auto world = MWBase::Environment::get().getWorld();

            if (wm->isGuiMode() && wm->isConsoleMode())
                return world->getTargetObject(result, pose.position.asMWUnits(), pose.orientation, world->getMaxActivationDistance() * 50, true);
            else
            {
                float activationDistance = 0.f;
                if (allowTelekinesis)
                    activationDistance = world->getActivationDistancePlusTelekinesis();
                else
                    activationDistance = world->getMaxActivationDistance();

                auto distance = world->getTargetObject(result, pose.position.asMWUnits(), pose.orientation, activationDistance, true);

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

        Stereo::Pose getNodePose(const osg::Node* node)
        {
            osg::Matrix worldMatrix = osg::computeLocalToWorld(node->getParentalNodePaths()[0]);
            Stereo::Pose pose;
            pose.position = Stereo::Position::fromMWUnits(worldMatrix.getTrans());
            pose.orientation = worldMatrix.getRotate();
            return pose;
        }
    }
}
