#include "action.hpp"

#include <components/xr/debug.hpp>
#include <components/xr/instance.hpp>

namespace XR
{
    Action::Action(
        XrAction action,
        XrActionType actionType,
        const std::string& actionName,
        const std::string& localName)
        : mAction(action)
        , mType(actionType)
        , mName(actionName)
        , mLocalName(localName)
    {
        XR::Debugging::setName(action, "OpenMW XR Action " + actionName);
    };

    Action::~Action() {
        if (mAction)
        {
            CHECK_XRCMD(xrDestroyAction(mAction));
        }
    }

    bool Action::getFloat(XrPath subactionPath, float& value)
    {
        XrActionStateGetInfo getInfo{ XR_TYPE_ACTION_STATE_GET_INFO };
        getInfo.action = mAction;
        getInfo.subactionPath = subactionPath;

        XrActionStateFloat xrValue{ XR_TYPE_ACTION_STATE_FLOAT };
        CHECK_XRCMD(xrGetActionStateFloat(XR::Instance::instance().xrSession(), &getInfo, &xrValue));

        if (xrValue.isActive)
            value = xrValue.currentState;
        return xrValue.isActive;
    }

    bool Action::getBool(XrPath subactionPath, bool& value)
    {
        XrActionStateGetInfo getInfo{ XR_TYPE_ACTION_STATE_GET_INFO };
        getInfo.action = mAction;
        getInfo.subactionPath = subactionPath;

        XrActionStateBoolean xrValue{ XR_TYPE_ACTION_STATE_BOOLEAN };
        CHECK_XRCMD(xrGetActionStateBoolean(XR::Instance::instance().xrSession(), &getInfo, &xrValue));

        if (xrValue.isActive)
            value = xrValue.currentState;
        return xrValue.isActive;
    }

    // Pose action only checks if the pose is active or not
    bool Action::getPoseIsActive(XrPath subactionPath)
    {
        XrActionStateGetInfo getInfo{ XR_TYPE_ACTION_STATE_GET_INFO };
        getInfo.action = mAction;
        getInfo.subactionPath = subactionPath;

        XrActionStatePose xrValue{ XR_TYPE_ACTION_STATE_POSE };
        CHECK_XRCMD(xrGetActionStatePose(XR::Instance::instance().xrSession(), &getInfo, &xrValue));

        return xrValue.isActive;
    }

    bool Action::applyHaptics(XrPath subactionPath, float amplitude)
    {
        amplitude = std::max(0.f, std::min(1.f, amplitude));

        XrHapticVibration vibration{ XR_TYPE_HAPTIC_VIBRATION };
        vibration.amplitude = amplitude;
        vibration.duration = XR_MIN_HAPTIC_DURATION;
        vibration.frequency = XR_FREQUENCY_UNSPECIFIED;

        XrHapticActionInfo hapticActionInfo{ XR_TYPE_HAPTIC_ACTION_INFO };
        hapticActionInfo.action = mAction;
        hapticActionInfo.subactionPath = subactionPath;
        CHECK_XRCMD(xrApplyHapticFeedback(XR::Instance::instance().xrSession(), &hapticActionInfo, (XrHapticBaseHeader*)&vibration));
        return true;
    }
}
