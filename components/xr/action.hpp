#ifndef XR_ACTION_HPP
#define XR_ACTION_HPP

#include <openxr/openxr.h>

#include <string>

namespace XR
{
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
}

#endif
