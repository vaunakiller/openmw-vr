#include "debug.hpp"
#include "instance.hpp"
#include "extensions.hpp"

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
    XrResult CheckXrResult(XrResult res, const char* originator, const char* sourceLocation) {
        static bool initialized = false;
        static bool sLogAllXrCalls = false;
        static bool sContinueOnErrors = false;
        if (!initialized)
        {
            initialized = true;
            sLogAllXrCalls = Settings::Manager::getBool("log all openxr calls", "VR Debug");
            sContinueOnErrors = Settings::Manager::getBool("continue on errors", "VR Debug");
        }

        auto resultString = XrResultString(res);

        if (XR_FAILED(res)) {
            std::stringstream ss;
#ifdef _WIN32
            ss << sourceLocation << ": OpenXR[Error: " << resultString << "][Thread: " << std::this_thread::get_id() << "]: " << originator;
#elif __linux__
            ss << sourceLocation << ": OpenXR[Error: " << resultString << "][Thread: " << std::this_thread::get_id() << "]: " << originator;
#endif
            Log(Debug::Error) << ss.str();
            if (!sContinueOnErrors)
                throw std::runtime_error(ss.str().c_str());
        }
        else if (res != XR_SUCCESS || sLogAllXrCalls)
        {
#ifdef _WIN32
            Log(Debug::Verbose) << sourceLocation << ": OpenXR[" << resultString << "][" << std::this_thread::get_id() << "]: " << originator;
#elif __linux__
            Log(Debug::Verbose) << sourceLocation << ": OpenXR[" << resultString << "][" << std::this_thread::get_id() << "]: " << originator;
#endif
        }

        return res;
    }

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

        if (Extensions::instance().extensionEnabled(XR_EXT_DEBUG_UTILS_EXTENSION_NAME))
        {
            XrDebugUtilsObjectNameInfoEXT nameInfo{ XR_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT, nullptr };
            nameInfo.objectHandle = handle;
            nameInfo.objectType = type;
            nameInfo.objectName = name.c_str();

            xrSetDebugUtilsObjectNameEXT(&nameInfo);
        }
    }



    static XrInstanceProperties
        getInstanceProperties(XrInstance instance)
    {
        XrInstanceProperties properties{ XR_TYPE_INSTANCE_PROPERTIES };
        if (instance)
            xrGetInstanceProperties(instance, &properties);
        return properties;
    }

    std::string getInstanceName(XrInstance instance)
    {
        if (instance)
            return getInstanceProperties(instance).runtimeName;
        return "unknown";
    }

    XrVersion getInstanceVersion(XrInstance instance)
    {
        if (instance)
            return getInstanceProperties(instance).runtimeVersion;
        return 0;
    }

    void initFailure(XrResult res, XrInstance instance)
    {
        std::stringstream ss;
        std::string runtimeName = getInstanceName(instance);
        XrVersion runtimeVersion = getInstanceVersion(instance);
        ss << "Error caught while initializing VR device: " << XrResultString(res) << std::endl;
        ss << "Device: " << runtimeName << std::endl;
        ss << "Version: " << runtimeVersion << std::endl;
        if (res == XR_ERROR_FORM_FACTOR_UNAVAILABLE)
        {
            ss << "Cause: Unable to open VR device. Make sure your device is plugged in and the VR driver is running." << std::endl;
            ss << std::endl;
            if (runtimeName == "Oculus" || runtimeName == "Quest")
            {
                ss << "Your device has been identified as an Oculus device." << std::endl;
                ss << "The most common cause for this error when using an oculus device, is quest users attempting to run the game via Virtual Desktop." << std::endl;
                ss << "Unfortunately this is currently broken, and quest users will need to play via a link cable." << std::endl;
            }
        }
        else if (res == XR_ERROR_LIMIT_REACHED)
        {
            ss << "Cause: Device resources exhausted. Close other VR applications if you have any open. If you have none, you may need to reboot to reset the driver." << std::endl;
        }
        else
        {
            ss << "Cause: Unknown. Make sure your device is plugged in and ready." << std::endl;
        }
        throw std::runtime_error(ss.str());
    }
}
