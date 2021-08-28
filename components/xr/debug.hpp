#ifndef OPENXR_DEBUG_HPP
#define OPENXR_DEBUG_HPP

#include <openxr/openxr.h>

#include <string>

namespace XR
{
    namespace Debugging
    {
        //! Translates an OpenXR object to the associated XrObjectType enum value
        template<typename T> XrObjectType getObjectType(T t);

        //! Associates a name with an OpenXR symbol if XR_EXT_debug_utils is enabled
        template<typename T> void setName(T t, const std::string& name);

        //! Associates a name with an OpenXR symbol if XR_EXT_debug_utils is enabled
        void setName(uint64_t handle, XrObjectType type, const std::string& name);
    }
}

template<typename T> inline void XR::Debugging::setName(T t, const std::string& name)
{
    setName(reinterpret_cast<uint64_t>(t), getObjectType(t), name);
}

template<> inline XrObjectType XR::Debugging::getObjectType<XrInstance>(XrInstance)
{
    return XR_OBJECT_TYPE_INSTANCE;
}

template<> inline XrObjectType XR::Debugging::getObjectType<XrSession>(XrSession)
{
    return XR_OBJECT_TYPE_SESSION;
}

template<> inline XrObjectType XR::Debugging::getObjectType<XrSpace>(XrSpace)
{
    return XR_OBJECT_TYPE_SPACE;
}

template<> inline XrObjectType XR::Debugging::getObjectType<XrActionSet>(XrActionSet)
{
    return XR_OBJECT_TYPE_ACTION_SET;
}

template<> inline XrObjectType XR::Debugging::getObjectType<XrAction>(XrAction)
{
    return XR_OBJECT_TYPE_ACTION;
}

template<> inline XrObjectType XR::Debugging::getObjectType<XrDebugUtilsMessengerEXT>(XrDebugUtilsMessengerEXT)
{
    return XR_OBJECT_TYPE_DEBUG_UTILS_MESSENGER_EXT;
}

template<> inline XrObjectType XR::Debugging::getObjectType<XrSpatialAnchorMSFT>(XrSpatialAnchorMSFT)
{
    return XR_OBJECT_TYPE_SPATIAL_ANCHOR_MSFT;
}

template<> inline XrObjectType XR::Debugging::getObjectType<XrHandTrackerEXT>(XrHandTrackerEXT)
{
    return XR_OBJECT_TYPE_HAND_TRACKER_EXT;
}

template<typename T> inline XrObjectType XR::Debugging::getObjectType(T t)
{
    return XR_OBJECT_TYPE_UNKNOWN;
}

#endif 
