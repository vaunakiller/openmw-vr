#include "widget.hpp"

#include <SDL_events.h>
#include <components/sdlutil/sdlmappings.hpp>

#include "text.hpp"
#include "textedit.hpp"
#include "window.hpp"

namespace LuaUi
{
    WidgetExtension::WidgetExtension()
        : mForcePosition(false)
        , mForceSize(false)
        , mPropagateEvents(true)
        , mLua(nullptr)
        , mWidget(nullptr)
        , mSlot(this)
        , mLayout(sol::nil)
        , mProperties(sol::nil)
        , mTemplateProperties(sol::nil)
        , mExternal(sol::nil)
        , mParent(nullptr)
        , mTemplateChild(false)
    {}

    void WidgetExtension::initialize(lua_State* lua, MyGUI::Widget* self)
    {
        mLua = lua;
        mWidget = self;
        initialize();
        updateTemplate();
    }

    void WidgetExtension::initialize()
    {
        // \todo might be more efficient to only register these if there are Lua callbacks
        registerEvents(mWidget);
    }

    void WidgetExtension::deinitialize()
    {
        clearCallbacks();
        clearEvents(mWidget);

        mOnCoordChange.reset();

        for (WidgetExtension* w : mChildren)
            w->deinitialize();
        for (WidgetExtension* w : mTemplateChildren)
            w->deinitialize();
    }



    void WidgetExtension::registerEvents(MyGUI::Widget* w)
    {
        if (!mEventsInitialized)
        {
            mEventKeyButtonPressed = MyGUI::newDelegate(this, &WidgetExtension::keyPress);
            mEventKeyButtonReleased = MyGUI::newDelegate(this, &WidgetExtension::keyRelease);
            mEventMouseButtonClick = MyGUI::newDelegate(this, &WidgetExtension::mouseClick);
            mEventMouseButtonDoubleClick = MyGUI::newDelegate(this, &WidgetExtension::mouseDoubleClick);
            mEventMouseButtonPressed = MyGUI::newDelegate(this, &WidgetExtension::mousePress);
            mEventMouseButtonReleased = MyGUI::newDelegate(this, &WidgetExtension::mouseRelease);
            mEventMouseMove = MyGUI::newDelegate(this, &WidgetExtension::mouseMove);
            mEventMouseDrag = MyGUI::newDelegate(this, &WidgetExtension::mouseDrag);
            mEventMouseSetFocus = MyGUI::newDelegate(this, &WidgetExtension::focusGain);
            mEventMouseLostFocus = MyGUI::newDelegate(this, &WidgetExtension::focusLoss);
            mEventKeySetFocusDelegate = MyGUI::newDelegate(this, &WidgetExtension::focusGain);
            mEventKeyLostFocusDelegate = MyGUI::newDelegate(this, &WidgetExtension::focusLoss);

            w->eventKeyButtonPressed += mEventKeyButtonPressed;
            w->eventKeyButtonReleased += mEventKeyButtonReleased;
            w->eventMouseButtonClick += mEventMouseButtonClick;
            w->eventMouseButtonDoubleClick += mEventMouseButtonDoubleClick;
            w->eventMouseButtonPressed += mEventMouseButtonPressed;
            w->eventMouseButtonReleased += mEventMouseButtonReleased;
            w->eventMouseMove += mEventMouseMove;
            w->eventMouseDrag += mEventMouseDrag;
            w->eventMouseSetFocus += mEventMouseSetFocus;
            w->eventMouseLostFocus += mEventMouseLostFocus;
            w->eventKeySetFocus += mEventKeySetFocusDelegate;
            w->eventKeyLostFocus += mEventKeyLostFocusDelegate;

            mEventsInitialized = true;
        }
    }
    void WidgetExtension::clearEvents(MyGUI::Widget* w)
    {
        if (mEventsInitialized)
        {
            w->eventKeyButtonPressed -= mEventKeyButtonPressed;
            w->eventKeyButtonReleased -= mEventKeyButtonReleased;
            w->eventMouseButtonClick -= mEventMouseButtonClick;
            w->eventMouseButtonDoubleClick -= mEventMouseButtonDoubleClick;
            w->eventMouseButtonPressed -= mEventMouseButtonPressed;
            w->eventMouseButtonReleased -= mEventMouseButtonReleased;
            w->eventMouseMove -= mEventMouseMove;
            w->eventMouseDrag -= mEventMouseDrag;
            w->eventMouseSetFocus -= mEventMouseSetFocus;
            w->eventMouseLostFocus -= mEventMouseLostFocus;
            w->eventKeySetFocus -= mEventKeySetFocusDelegate;
            w->eventKeyLostFocus -= mEventKeyLostFocusDelegate;

            mEventsInitialized = false;
        }
    }

    void WidgetExtension::reset()
    {
        // detach all children from the slot widget, in case it gets destroyed
        for (auto& w: mChildren)
            w->widget()->detachFromWidget();
    }

    void WidgetExtension::attach(WidgetExtension* ext)
    {
        ext->mParent = this;
        ext->mTemplateChild = false;
        ext->widget()->attachToWidget(mSlot->widget());
        // workaround for MyGUI bug
        // parent visibility doesn't affect added children
        ext->widget()->setVisible(!ext->widget()->getVisible());
        ext->widget()->setVisible(!ext->widget()->getVisible());
    }

    void WidgetExtension::attachTemplate(WidgetExtension* ext)
    {
        ext->mParent = this;
        ext->mTemplateChild = true;
        ext->widget()->attachToWidget(widget());
        // workaround for MyGUI bug
        // parent visibility doesn't affect added children
        ext->widget()->setVisible(!ext->widget()->getVisible());
        ext->widget()->setVisible(!ext->widget()->getVisible());
    }

    WidgetExtension* WidgetExtension::findDeep(std::string_view flagName)
    {
        for (WidgetExtension* w : mChildren)
        {
            WidgetExtension* result = w->findDeep(flagName);
            if (result != nullptr)
                return result;
        }
        if (externalValue(flagName, false))
            return this;
        return nullptr;
    }

    void WidgetExtension::findAll(std::string_view flagName, std::vector<WidgetExtension*>& result)
    {
        if (externalValue(flagName, false))
            result.push_back(this);
        for (WidgetExtension* w : mChildren)
            w->findAll(flagName, result);
    }

    WidgetExtension* WidgetExtension::findDeepInTemplates(std::string_view flagName)
    {
        for (WidgetExtension* w : mTemplateChildren)
        {
            WidgetExtension* result = w->findDeep(flagName);
            if (result != nullptr)
                return result;
        }
        return nullptr;
    }

    std::vector<WidgetExtension*> WidgetExtension::findAllInTemplates(std::string_view flagName)
    {
        std::vector<WidgetExtension*> result;
        for (WidgetExtension* w : mTemplateChildren)
            w->findAll(flagName, result);
        return result;
    }

    sol::table WidgetExtension::makeTable() const
    {
        return sol::table(lua(), sol::create);
    }

    sol::object WidgetExtension::keyEvent(MyGUI::KeyCode code) const
    {
        auto keySym = SDL_Keysym();
        keySym.sym = SDLUtil::myGuiKeyToSdl(code);
        keySym.scancode = SDL_GetScancodeFromKey(keySym.sym);
        keySym.mod = SDL_GetModState();
        return sol::make_object(lua(), keySym);
    }

    sol::object WidgetExtension::mouseEvent(int left, int top, MyGUI::MouseButton button = MyGUI::MouseButton::None) const
    {
        osg::Vec2f position(left, top);
        MyGUI::IntPoint absolutePosition = mWidget->getAbsolutePosition();
        osg::Vec2f offset = position - osg::Vec2f(absolutePosition.left, absolutePosition.top);
        sol::table table = makeTable();
        int sdlButton = SDLUtil::myGuiMouseButtonToSdl(button);
        table["position"] = position;
        table["offset"] = offset;
        if (sdlButton != 0) // nil if no button was pressed
            table["button"] = sdlButton;
        return table;
    }

    void WidgetExtension::setChildren(const std::vector<WidgetExtension*>& children)
    {
        mChildren.resize(children.size());
        for (size_t i = 0; i < children.size(); ++i)
        {
            mChildren[i] = children[i];
            attach(mChildren[i]);
        }
        updateChildren();
    }

    void WidgetExtension::setTemplateChildren(const std::vector<WidgetExtension*>& children)
    {
        mTemplateChildren.resize(children.size());
        for (size_t i = 0; i < children.size(); ++i)
        {
            mTemplateChildren[i] = children[i];
            attachTemplate(mTemplateChildren[i]);
        }
        updateTemplate();
    }

    void WidgetExtension::updateTemplate()
    {
        WidgetExtension* slot = findDeepInTemplates("slot");
        if (slot == nullptr)
            mSlot = this;
        else
            mSlot = slot->mSlot;
    }

    void WidgetExtension::setCallback(const std::string& name, const LuaUtil::Callback& callback)
    {
        mCallbacks[name] = callback;
    }

    void WidgetExtension::clearCallbacks()
    {
        mCallbacks.clear();
    }

    MyGUI::IntCoord WidgetExtension::forcedCoord()
    {
        return mForcedCoord;
    }

    void WidgetExtension::forceCoord(const MyGUI::IntCoord& offset)
    {
        mForcePosition = true;
        mForceSize = true;
        mForcedCoord = offset;
    }

    void WidgetExtension::forcePosition(const MyGUI::IntPoint& pos)
    {
        mForcePosition = true;
        mForcedCoord = pos;
    }

    void WidgetExtension::forceSize(const MyGUI::IntSize& size)
    {
        mForceSize = true;
        mForcedCoord = size;
    }

    void WidgetExtension::clearForced() {
        mForcePosition = false;
        mForceSize = false;
    }

    void WidgetExtension::updateCoord()
    {
        MyGUI::IntCoord oldCoord = mWidget->getCoord();
        MyGUI::IntCoord newCoord = calculateCoord();

        if (oldCoord != newCoord)
            mWidget->setCoord(newCoord);
        updateChildrenCoord();
        if (oldCoord != newCoord && mOnCoordChange.has_value())
            mOnCoordChange.value()(this, newCoord);
    }

    void WidgetExtension::setProperties(sol::object props)
    {
        mProperties = props;
        updateProperties();
    }

    void WidgetExtension::updateProperties()
    {
        mPropagateEvents = propertyValue("propagateEvents", true);
        mAbsoluteCoord = propertyValue("position", MyGUI::IntPoint());
        mAbsoluteCoord = propertyValue("size", MyGUI::IntSize());
        mRelativeCoord = propertyValue("relativePosition", MyGUI::FloatPoint());
        mRelativeCoord = propertyValue("relativeSize", MyGUI::FloatSize());
        mAnchor = propertyValue("anchor", MyGUI::FloatSize());
        mWidget->setVisible(propertyValue("visible", true));
        mWidget->setPointer(propertyValue("pointer", std::string("arrow")));
        mWidget->setAlpha(propertyValue("alpha", 1.f));
        mWidget->setInheritsAlpha(propertyValue("inheritAlpha", true));
    }

    void WidgetExtension::updateChildrenCoord()
    {
        for (WidgetExtension* w : mTemplateChildren)
            w->updateCoord();
        for (WidgetExtension* w : mChildren)
            w->updateCoord();
    }

    MyGUI::IntSize WidgetExtension::parentSize()
    {
        if (!mParent)
            return widget()->getParentSize(); // size of the layer
        if (mTemplateChild)
            return mParent->templateScalingSize();
        else
            return mParent->childScalingSize();
    }

    MyGUI::IntSize WidgetExtension::calculateSize()
    {
        if (mForceSize)
            return mForcedCoord.size();

        MyGUI::IntSize pSize = parentSize();
        MyGUI::IntSize newSize;
        newSize = mAbsoluteCoord.size();
        newSize.width += mRelativeCoord.width * pSize.width;
        newSize.height += mRelativeCoord.height * pSize.height;
        return newSize;
    }

    MyGUI::IntPoint WidgetExtension::calculatePosition(const MyGUI::IntSize& size)
    {
        if (mForcePosition)
            return mForcedCoord.point();
        MyGUI::IntSize pSize = parentSize();
        MyGUI::IntPoint newPosition;
        newPosition = mAbsoluteCoord.point();
        newPosition.left += mRelativeCoord.left * pSize.width - mAnchor.width * size.width;
        newPosition.top += mRelativeCoord.top * pSize.height - mAnchor.height * size.height;
        return newPosition;
    }

    MyGUI::IntCoord WidgetExtension::calculateCoord()
    {
        MyGUI::IntCoord newCoord;
        newCoord = calculateSize();
        newCoord = calculatePosition(newCoord.size());
        return newCoord;
    }

    MyGUI::IntSize WidgetExtension::childScalingSize()
    {
        return mSlot->widget()->getSize();
    }

    MyGUI::IntSize WidgetExtension::templateScalingSize()
    {
        return widget()->getSize();
    }

    void WidgetExtension::triggerEvent(std::string_view name, sol::object argument) const
    {
        auto it = mCallbacks.find(name);
        if (it != mCallbacks.end())
            it->second.call(argument, mLayout);
    }

    void WidgetExtension::keyPress(MyGUI::Widget*, MyGUI::KeyCode code, MyGUI::Char ch)
    {
        if (code == MyGUI::KeyCode::None)
        {
            propagateEvent("textInput", [ch](auto w) {
                MyGUI::UString uString;
                uString.push_back(static_cast<MyGUI::UString::unicode_char>(ch));
                return sol::make_object(w->lua(), uString.asUTF8());
            });
        }
        else
            propagateEvent("keyPress", [code](auto w){ return w->keyEvent(code); });
    }

    void WidgetExtension::keyRelease(MyGUI::Widget*, MyGUI::KeyCode code)
    {
        propagateEvent("keyRelease", [code](auto w) { return w->keyEvent(code); });
    }

    void WidgetExtension::mouseMove(MyGUI::Widget*, int left, int top)
    {
        propagateEvent("mouseMove", [left, top](auto w) { return w->mouseEvent(left, top); });
    }

    void WidgetExtension::mouseDrag(MyGUI::Widget*, int left, int top, MyGUI::MouseButton button)
    {
        propagateEvent("mouseMove", [left, top, button](auto w) { return w->mouseEvent(left, top, button); });
    }

    void WidgetExtension::mouseClick(MyGUI::Widget* _widget)
    {
        propagateEvent("mouseClick", [](auto){ return sol::nil; });
    }

    void WidgetExtension::mouseDoubleClick(MyGUI::Widget* _widget)
    {
        propagateEvent("mouseDoubleClick", [](auto){ return sol::nil; });
    }

    void WidgetExtension::mousePress(MyGUI::Widget*, int left, int top, MyGUI::MouseButton button)
    {
        propagateEvent("mousePress", [left, top, button](auto w) { return w->mouseEvent(left, top, button); });
    }

    void WidgetExtension::mouseRelease(MyGUI::Widget*, int left, int top, MyGUI::MouseButton button)
    {
        propagateEvent("mouseRelease", [left, top, button](auto w) { return w->mouseEvent(left, top, button); });
    }

    void WidgetExtension::focusGain(MyGUI::Widget*, MyGUI::Widget*)
    {
        propagateEvent("focusGain", [](auto){ return sol::nil; });
    }

    void WidgetExtension::focusLoss(MyGUI::Widget*, MyGUI::Widget*)
    {
        propagateEvent("focusLoss", [](auto){ return sol::nil; });
    }
}
