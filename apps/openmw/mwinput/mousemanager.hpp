#ifndef MWINPUT_MWMOUSEMANAGER_H
#define MWINPUT_MWMOUSEMANAGER_H

#include <components/settings/settings.hpp>
#include <components/sdlutil/events.hpp>

namespace SDLUtil
{
    class InputWrapper;
}

namespace MWInput
{
    class BindingsManager;

    class MouseManager : public SDLUtil::MouseListener
    {
    public:
        MouseManager(BindingsManager* bindingsManager, SDLUtil::InputWrapper* inputWrapper, SDL_Window* window);

        virtual ~MouseManager() = default;

        void updateCursorMode();
        void update(float dt);

        void mouseMoved(const SDLUtil::MouseMotionEvent &arg) override;
        void mousePressed(const SDL_MouseButtonEvent &arg, Uint8 id) override;
        void mouseReleased(const SDL_MouseButtonEvent &arg, Uint8 id) override;
        void mouseWheelMoved(const SDL_MouseWheelEvent &arg) override;

        void processChangedSettings(const Settings::CategorySettingVector& changed);

        bool injectMouseButtonPress(Uint8 button);
        bool injectMouseButtonRelease(Uint8 button);
        void injectMouseMove(float xMove, float yMove, float mouseWheelMove);
        void warpMouse();

        void setMouseLookEnabled(bool enabled) { mMouseLookEnabled = enabled; }
        void setGuiCursorEnabled(bool enabled) { mGuiCursorEnabled = enabled; }

        int getMouseMoveX() const { return mMouseMoveX; }
        int getMouseMoveY() const { return mMouseMoveY; }

        // Used to override mouse position when using controllers not through SDL, such as OpenXR.
        void setMousePosition(int x, int y);

    private:
        bool mInvertX;
        bool mInvertY;
        bool mGrabCursor;
        float mCameraSensitivity;
        float mCameraYMultiplier;

        BindingsManager* mBindingsManager;
        SDLUtil::InputWrapper* mInputWrapper;

        float mGuiCursorX;
        float mGuiCursorY;
        int mMouseWheel;
        bool mMouseLookEnabled;
        bool mGuiCursorEnabled;

        int mMouseMoveX;
        int mMouseMoveY;

        float mPreviousXAxis;
    };
}
#endif
