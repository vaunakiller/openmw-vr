#include "action.hpp"
#include "session.hpp"
#include <components/xr/debug.hpp>
#include <components/xr/instance.hpp>

#include <cassert>

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
    }

    Action::~Action() {
        if (mAction)
        {
            CHECK_XRCMD(xrDestroyAction(mAction));
        }
    }

    bool Action::getFloat(XrPath subactionPath, float& value)
    {
        XrActionStateGetInfo getInfo{};
        getInfo.type = XR_TYPE_ACTION_STATE_GET_INFO;
        getInfo.action = mAction;
        getInfo.subactionPath = subactionPath;

        XrActionStateFloat xrValue{};
        xrValue.type = XR_TYPE_ACTION_STATE_FLOAT;
        CHECK_XRCMD(xrGetActionStateFloat(XR::Session::instance().xrSession(), &getInfo, &xrValue));

        if (xrValue.isActive)
            value = xrValue.currentState;
        return xrValue.isActive;
    }

    bool Action::getBool(XrPath subactionPath, bool& value)
    {
        XrActionStateGetInfo getInfo{};
        getInfo.type = XR_TYPE_ACTION_STATE_GET_INFO;
        getInfo.action = mAction;
        getInfo.subactionPath = subactionPath;

        XrActionStateBoolean xrValue{};
        xrValue.type = XR_TYPE_ACTION_STATE_BOOLEAN;
        CHECK_XRCMD(xrGetActionStateBoolean(XR::Session::instance().xrSession(), &getInfo, &xrValue));

        if (xrValue.isActive)
            value = xrValue.currentState;
        return xrValue.isActive;
    }

    // Pose action only checks if the pose is active or not
    bool Action::getPoseIsActive(XrPath subactionPath)
    {
        XrActionStateGetInfo getInfo{};
        getInfo.type = XR_TYPE_ACTION_STATE_GET_INFO;
        getInfo.action = mAction;
        getInfo.subactionPath = subactionPath;

        XrActionStatePose xrValue{};
        xrValue.type = XR_TYPE_ACTION_STATE_POSE;
        CHECK_XRCMD(xrGetActionStatePose(XR::Session::instance().xrSession(), &getInfo, &xrValue));

        return xrValue.isActive;
    }

    bool Action::applyHaptics(XrPath subactionPath, float amplitude)
    {
        amplitude = std::max(0.f, std::min(1.f, amplitude));

        XrHapticVibration vibration{};
        vibration.type = XR_TYPE_HAPTIC_VIBRATION;
        vibration.amplitude = amplitude;
        vibration.duration = XR_MIN_HAPTIC_DURATION;
        vibration.frequency = XR_FREQUENCY_UNSPECIFIED;

        XrHapticActionInfo hapticActionInfo{};
        hapticActionInfo.type = XR_TYPE_HAPTIC_ACTION_INFO;
        hapticActionInfo.action = mAction;
        hapticActionInfo.subactionPath = subactionPath;
        CHECK_XRCMD(xrApplyHapticFeedback(XR::Session::instance().xrSession(), &hapticActionInfo, (XrHapticBaseHeader*)&vibration));
        return true;
    }

    //! Delay before a long-press action is activated (and regular press is discarded)
    //! TODO: Make this configurable?
    static std::chrono::milliseconds gActionTime{ 666 };
    //! Magnitude above which an axis action is considered active
    static float gAxisEpsilon{ 0.01f };

    void HapticsAction::apply(float amplitude)
    {
        mAmplitude = std::max(0.f, std::min(1.f, amplitude));
        mXRAction->applyHaptics(XR_NULL_PATH, mAmplitude);
    }

    PoseAction::PoseAction(std::unique_ptr<XR::Action> xrAction)
        : mXRAction(std::move(xrAction))
        , mXRSpace{ XR_NULL_HANDLE }
    {
        XrActionSpaceCreateInfo createInfo{};
        createInfo.type = XR_TYPE_ACTION_SPACE_CREATE_INFO;
        createInfo.action = mXRAction->xrAction();
        createInfo.poseInActionSpace.orientation.w = 1.f;
        createInfo.subactionPath = XR_NULL_PATH;
        CHECK_XRCMD(xrCreateActionSpace(XR::Session::instance().xrSession(), &createInfo, &mXRSpace));
        XR::Debugging::setName(mXRSpace, "OpenMW XR Action Space " + mXRAction->mName);
    }

    void InputAction::updateAndQueue(std::deque<const InputAction*>& queue)
    {
        bool old = mActive;
        mPrevious = mValue;
        update();
        bool changed = old != mActive;
        mOnActivate = changed && mActive;
        mOnDeactivate = changed && !mActive;

        if (shouldQueue())
        {
            queue.push_back(this);
        }
    }

    void ButtonPressAction::update()
    {
        mActive = false;
        bool old = mPressed;
        mXRAction->getBool(0, mPressed);
        bool changed = old != mPressed;
        if (changed && mPressed)
        {
            mPressTime = std::chrono::steady_clock::now();
            mTimeout = mPressTime + gActionTime;
        }
        if (changed && !mPressed)
        {
            if (std::chrono::steady_clock::now() < mTimeout)
            {
                mActive = true;
            }
        }

        mValue = mPressed ? 1.f : 0.f;
    }

    void ButtonLongPressAction::update()
    {
        mActive = false;
        bool old = mPressed;
        mXRAction->getBool(0, mPressed);
        bool changed = old != mPressed;
        if (changed && mPressed)
        {
            mPressTime = std::chrono::steady_clock::now();
            mTimein = mPressTime + gActionTime;
            mActivated = false;
        }
        if (mPressed && !mActivated)
        {
            if (std::chrono::steady_clock::now() >= mTimein)
            {
                mActive = mActivated = true;
                mValue = 1.f;
            }
        }
        if (changed && !mPressed)
        {
            mValue = 0.f;
        }
    }


    void ButtonHoldAction::update()
    {
        mXRAction->getBool(0, mPressed);
        mActive = mPressed;

        mValue = mPressed ? 1.f : 0.f;
    }


    AxisAction::AxisAction(int openMWAction, std::unique_ptr<XR::Action> xrAction, std::shared_ptr<AxisAction::Deadzone> deadzone)
        : InputAction(openMWAction, std::move(xrAction))
        , mDeadzone(deadzone)
    {
    }

    void AxisAction::update()
    {
        mActive = false;
        mXRAction->getFloat(0, mValue);
        mDeadzone->applyDeadzone(mValue);

        if (std::fabs(mValue) > gAxisEpsilon)
            mActive = true;
        else
            mValue = 0.f;
    }

    void AxisAction::Deadzone::applyDeadzone(float& value)
    {
        float sign = std::copysignf(1.f, value);
        float magnitude = std::fabs(value);
        magnitude = std::min(mActiveRadiusOuter, magnitude);
        magnitude = std::max(0.f, magnitude - mActiveRadiusInner);
        value = sign * magnitude * mActiveScale;

    }

    void AxisAction::Deadzone::setDeadzoneRadius(float deadzoneRadius)
    {
        deadzoneRadius = std::min(std::max(deadzoneRadius, 0.0f), 0.5f - 1e-5f);
        mActiveRadiusInner = deadzoneRadius;
        mActiveRadiusOuter = 1.f - deadzoneRadius;
        float activeRadius = mActiveRadiusOuter - mActiveRadiusInner;
        assert(activeRadius > 0.f);
        mActiveScale = 1.f / activeRadius;
    }
}
