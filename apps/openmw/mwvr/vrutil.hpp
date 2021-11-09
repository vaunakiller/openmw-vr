#ifndef VR_UTIL_HPP
#define VR_UTIL_HPP

#include "../mwworld/ptr.hpp"

#include <components/misc/stereo.hpp>

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
        float getPoseTarget(MWRender::RayResult& result, const Misc::Pose& pose, bool allowTelekinesis);
        Misc::Pose getWeaponPose();
        Misc::Pose getNodePose(const osg::Node* node);
        void requestRecenter(bool resetZ);
    }

}

#endif
