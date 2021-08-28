#ifndef XR_ACTION_HPP
#define XR_ACTION_HPP

#include <openxr/openxr.h>

#include <string>
#include <deque>
#include <memory>
#include <chrono>

namespace XR
{
    enum class ControlType
    {
        Press,
        LongPress,
        Hold,
        Axis
    };

    /// \brief C++ wrapper for the XrAction type
    class Action
    {
        Action(const Action&) = delete;
        Action& operator=(const Action&) = delete;

    public:
        Action(XrAction action, XrActionType actionType, const std::string& actionName, const std::string& localName);
        ~Action();

        XrAction xrAction() { return mAction; }

        bool getFloat(XrPath subactionPath, float& value);
        bool getBool(XrPath subactionPath, bool& value);
        bool getPoseIsActive(XrPath subactionPath);
        bool applyHaptics(XrPath subactionPath, float amplitude);

        XrAction mAction = XR_NULL_HANDLE;
        XrActionType mType;
        std::string mName;
        std::string mLocalName;
    };

    /// \brief Action for applying haptics
    class HapticsAction
    {
    public:
        HapticsAction(std::unique_ptr<XR::Action> xrAction) : mXRAction{ std::move(xrAction) } {};

        //! Apply vibration at the given amplitude
        void apply(float amplitude);

        //! Convenience
        XrAction xrAction() { return mXRAction->xrAction(); }

    private:
        std::unique_ptr<XR::Action> mXRAction;
        float mAmplitude{ 0.f };
    };

    /// \brief Action for capturing tracking information
    class PoseAction
    {
    public:
        PoseAction(std::unique_ptr<XR::Action> xrAction);

        //! Convenience
        XrAction xrAction() { return mXRAction->xrAction(); }

        //! Action space
        XrSpace xrSpace() { return mXRSpace; }

    private:
        std::unique_ptr<XR::Action> mXRAction;
        XrSpace mXRSpace;
    };

    /// \brief Generic action
    /// \sa ButtonPressAction ButtonLongPressAction ButtonHoldAction AxisAction
    class InputAction
    {
    public:
        InputAction(int openMWAction, std::unique_ptr<Action> xrAction) : mXRAction(std::move(xrAction)), mOpenMWAction(openMWAction) {}
        virtual ~InputAction() {};

        //! True if action changed to being released in the last update
        bool isActive() const { return mActive; };

        //! True if activation turned on this update (i.e. onPress)
        bool onActivate() const { return mOnActivate; }

        //! True if activation turned off this update (i.e. onRelease)
        bool onDeactivate() const { return mOnDeactivate; }

        //! OpenMW Action code of this action
        int openMWActionCode() const { return mOpenMWAction; }

        //! Current value of an axis or lever action
        float value() const { return mValue; }

        //! Previous value
        float previousValue() const { return mPrevious; }

        //! Update internal states. Note that subclasses maintain both mValue and mActivate to allow
        //! axis and press to subtitute one another.
        virtual void update() = 0;

        //! Determines if an action update should be queued
        virtual bool shouldQueue() const = 0;

        //! Convenience
        XrAction xrAction() { return mXRAction->xrAction(); }

        //! Update and queue action if applicable
        void updateAndQueue(std::deque<const InputAction*>& queue);

    protected:
        std::unique_ptr<XR::Action> mXRAction;
        int mOpenMWAction;
        float mValue{ 0.f };
        float mPrevious{ 0.f };
        bool mActive{ false };
        bool mOnActivate{ false };
        bool mOnDeactivate{ false };
    };

    //! Action that activates once on release.
    //! Times out if the button is held longer than gHoldDelay.
    class ButtonPressAction : public InputAction
    {
    public:
        using InputAction::InputAction;

        static const XrActionType ActionType = XR_ACTION_TYPE_BOOLEAN_INPUT;

        void update() override;

        virtual bool shouldQueue() const override { return onActivate() || onDeactivate(); }

        bool mPressed{ false };
        std::chrono::steady_clock::time_point mPressTime{};
        std::chrono::steady_clock::time_point mTimeout{};
    };

    //! Action that activates once on a long press
    //! The press time is the same as the timeout for a regular press, allowing keys with double roles.
    class ButtonLongPressAction : public InputAction
    {
    public:
        using InputAction::InputAction;

        static const XrActionType ActionType = XR_ACTION_TYPE_BOOLEAN_INPUT;

        void update() override;

        virtual bool shouldQueue() const override { return onActivate() || onDeactivate(); }

        bool mPressed{ false };
        bool mActivated{ false };
        std::chrono::steady_clock::time_point mPressTime{};
        std::chrono::steady_clock::time_point mTimein{};
    };

    //! Action that is active whenever the button is pressed down.
    //! Useful for e.g. non-toggling sneak and automatically repeating actions
    class ButtonHoldAction : public InputAction
    {
    public:
        using InputAction::InputAction;

        static const XrActionType ActionType = XR_ACTION_TYPE_BOOLEAN_INPUT;

        void update() override;

        virtual bool shouldQueue() const override { return mActive || onDeactivate(); }

        bool mPressed{ false };
    };

    //! Action for axis actions, such as thumbstick axes or certain triggers/squeeze levers.
    //! Float axis are considered active whenever their magnitude is greater than gAxisEpsilon. This is useful
    //! as a touch subtitute on levers without touch.
    class AxisAction : public InputAction
    {
    public:
        class Deadzone
        {
        public:
            void applyDeadzone(float& value);

            void setDeadzoneRadius(float deadzoneRadius);

        private:
            float mActiveRadiusInner{ 0.f };
            float mActiveRadiusOuter{ 1.f };
            float mActiveScale{ 1.f };
        };

    public:
        AxisAction(int openMWAction, std::unique_ptr<XR::Action> xrAction, std::shared_ptr<AxisAction::Deadzone> deadzone);

        static const XrActionType ActionType = XR_ACTION_TYPE_FLOAT_INPUT;

        void update() override;

        virtual bool shouldQueue() const override { return mActive || onDeactivate(); }

        std::shared_ptr<AxisAction::Deadzone> mDeadzone;
    };
}

#endif
