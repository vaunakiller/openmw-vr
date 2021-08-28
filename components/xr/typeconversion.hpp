#ifndef XR_TYPECONVERSIONS_H
#define XR_TYPECONVERSIONS_H

#include <openxr/openxr.h>
#include <components/misc/stereo.hpp>
#include <osg/Vec3>
#include <osg/Quat>

namespace XR
{
    /// Conversion methods between openxr types to osg/mwvr types. Includes managing the differing conventions.
    Misc::Pose          fromXR(XrPosef pose);
    Misc::FieldOfView   fromXR(XrFovf fov);
    osg::Vec3           fromXR(XrVector3f);
    osg::Quat           fromXR(XrQuaternionf quat);
    XrPosef             toXR(Misc::Pose pose);
    XrFovf              toXR(Misc::FieldOfView fov);
    XrVector3f          toXR(osg::Vec3 v);
    XrQuaternionf       toXR(osg::Quat quat);
}

#endif
