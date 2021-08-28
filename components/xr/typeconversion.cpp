#include "typeconversion.hpp"
#include <iostream>
#include <components/vr/layer.hpp>

namespace XR
{
    osg::Vec3 fromXR(XrVector3f v)
    {
        return osg::Vec3{ v.x, -v.z, v.y };
    }

    osg::Quat fromXR(XrQuaternionf quat)
    {
        return osg::Quat{ quat.x, -quat.z, quat.y, quat.w };
    }

    XrVector3f toXR(osg::Vec3 v)
    {
        return XrVector3f{ v.x(), v.z(), -v.y() };
    }

    XrQuaternionf toXR(osg::Quat quat)
    {
        // OpenXR runtimes may fail or even throw an exception if a quaternion is malformed.
        if (std::fabs(quat.length()) < 0.01)
            quat = osg::Quat(0.f, 0.f, 0.f, 1.f);

        return XrQuaternionf{ static_cast<float>(quat.x()), static_cast<float>(quat.z()), static_cast<float>(-quat.y()), static_cast<float>(quat.w()) };
    }

    Misc::Pose fromXR(XrPosef pose)
    {
        return Misc::Pose{ fromXR(pose.position), fromXR(pose.orientation) };
    }

    XrPosef toXR(Misc::Pose pose)
    {
        return XrPosef{ toXR(pose.orientation), toXR(pose.position) };
    }

    Misc::FieldOfView fromXR(XrFovf fov)
    {
        return Misc::FieldOfView{ fov.angleLeft, fov.angleRight, fov.angleUp, fov.angleDown };
    }

    XrFovf toXR(Misc::FieldOfView fov)
    {
        return XrFovf{ fov.angleLeft, fov.angleRight, fov.angleUp, fov.angleDown };
    }
}


