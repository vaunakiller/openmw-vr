#ifndef VR_UTIL_HPP
#define VR_UTIL_HPP

#include "../mwworld/ptr.hpp"

#include <components/stereo/stereomanager.hpp>

namespace MWRender
{
    struct RayResult;
}

namespace MWVR
{
    namespace Util {
        std::pair<MWWorld::Ptr, float> getPointerTarget();
        std::pair<MWWorld::Ptr, float> getTouchTarget();
        std::pair<MWWorld::Ptr, float> getWeaponTarget();
        float getPoseTarget(MWRender::RayResult& result, const Stereo::Pose& pose, bool allowTelekinesis);
        Stereo::Pose getWeaponPose();
        Stereo::Pose getNodePose(const osg::Node* node);
    }

}

#endif
