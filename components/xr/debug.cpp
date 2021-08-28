#include "debug.hpp"
#include "instance.hpp"

// The OpenXR SDK's platform headers assume we've included these windows headers
#ifdef _WIN32
#include <Windows.h>
#include <objbase.h>

#elif __linux__
#include <X11/Xlib.h>
#include <GL/glx.h>
#undef None

#else
#error Unsupported platform
#endif

#include <openxr/openxr_platform_defines.h>
#include <openxr/openxr_reflection.h>

namespace XR
{
    static void xrSetDebugUtilsObjectNameEXT(XrDebugUtilsObjectNameInfoEXT* nameInfo)
    {
        static PFN_xrSetDebugUtilsObjectNameEXT setDebugUtilsObjectNameEXT = nullptr;
        if (setDebugUtilsObjectNameEXT == nullptr)
        {
            setDebugUtilsObjectNameEXT = reinterpret_cast<PFN_xrSetDebugUtilsObjectNameEXT>(Instance::instance().xrGetFunction("xrSetDebugUtilsObjectNameEXT"));
        }
        CHECK_XRCMD(setDebugUtilsObjectNameEXT(Instance::instance().xrInstance(), nameInfo));
    }

    void Debugging::setName(uint64_t handle, XrObjectType type, const std::string& name)
    {

        if (Instance::instance().xrExtensionIsEnabled(XR_EXT_DEBUG_UTILS_EXTENSION_NAME))
        {
            XrDebugUtilsObjectNameInfoEXT nameInfo{ XR_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT, nullptr };
            nameInfo.objectHandle = handle;
            nameInfo.objectType = type;
            nameInfo.objectName = name.c_str();

            xrSetDebugUtilsObjectNameEXT(&nameInfo);
        }
    }

}
