#ifndef MWGUI_SETTINGS_H
#define MWGUI_SETTINGS_H

#include <components/lua_ui/adapter.hpp>

#include "windowbase.hpp"

namespace MWGui
{
    class SettingsWindow : public WindowBase
    {
        public:
            SettingsWindow();

            void onOpen() override;

            void updateControlsBox();

            void updateLightSettings();

            void updateWindowModeSettings();

            void onResChange(int, int) override { center(); }

    protected:
            MyGUI::TabControl* mSettingsTab;
            MyGUI::Button* mOkButton;

            // VR
            MyGUI::ComboBox* mVRHudPosition;
            MyGUI::ComboBox* mVRTooltipPosition;
            MyGUI::ComboBox* mVRMirrorTextureEye;
            MyGUI::ComboBox* mVRSnapAngle;
            MyGUI::ComboBox* mVRThumbstickUp;
            MyGUI::ComboBox* mVRThumbstickDown;
            MyGUI::Button*   mVRHeightCalibButton;

            // graphics
            MyGUI::ListBox* mResolutionList;
            MyGUI::ComboBox* mWindowModeList;
            MyGUI::Button* mWindowBorderButton;
            MyGUI::ComboBox* mTextureFilteringButton;

            MyGUI::ComboBox* mWaterTextureSize;
            MyGUI::ComboBox* mWaterReflectionDetail;
            MyGUI::ComboBox* mWaterRainRippleDetail;

            MyGUI::ComboBox* mMaxLights;
            MyGUI::ComboBox* mLightingMethodButton;
            MyGUI::Button* mLightsResetButton;

            MyGUI::ComboBox* mPrimaryLanguage;
            MyGUI::ComboBox* mSecondaryLanguage;

            // controls
            MyGUI::ScrollView* mControlsBox;
            MyGUI::Button* mResetControlsButton;
            MyGUI::Button* mKeyboardSwitch;
            MyGUI::Button* mControllerSwitch;
            bool mKeyboardMode; //if true, setting up the keyboard. Otherwise, it's controller

            MyGUI::EditBox* mScriptFilter;
            MyGUI::ListBox* mScriptList;
            MyGUI::Widget* mScriptBox;
            MyGUI::Widget* mScriptDisabled;
            MyGUI::ScrollView* mScriptView;
            LuaUi::LuaAdapter* mScriptAdapter;
            int mCurrentPage;

            void onTabChanged(MyGUI::TabControl* _sender, size_t index);
            void onOkButtonClicked(MyGUI::Widget* _sender);
            void onTextureFilteringChanged(MyGUI::ComboBox* _sender, size_t pos);
            void onSliderChangePosition(MyGUI::ScrollBar* scroller, size_t pos);
            void onButtonToggled(MyGUI::Widget* _sender);
            void onResolutionSelected(MyGUI::ListBox* _sender, size_t index);
            void onResolutionAccept();
            void onResolutionCancel();
            void highlightCurrentResolution();

            void onVRMirrorTextureEyeChanged(MyGUI::ComboBox* _sender, size_t pos);
            void onVRHudPositionChanged(MyGUI::ComboBox* _sender, size_t pos);
            void onVRTooltipPositionChanged(MyGUI::ComboBox* _sender, size_t pos);
            void onVRSnapAngleChanged(MyGUI::ComboBox* _sender, size_t pos);
            void onVRThumbstickUpChanged(MyGUI::ComboBox* _sender, size_t pos);
            void onVRThumbstickDownChanged(MyGUI::ComboBox* _sender, size_t pos);
            void onVRHeightCalibButtonClicked(MyGUI::Widget* _sender);

            void onWaterTextureSizeChanged(MyGUI::ComboBox* _sender, size_t pos);
            void onWaterReflectionDetailChanged(MyGUI::ComboBox* _sender, size_t pos);
            void onWaterRainRippleDetailChanged(MyGUI::ComboBox* _sender, size_t pos);

            void onLightingMethodButtonChanged(MyGUI::ComboBox* _sender, size_t pos);
            void onLightsResetButtonClicked(MyGUI::Widget* _sender);
            void onMaxLightsChanged(MyGUI::ComboBox* _sender, size_t pos);

            void onPrimaryLanguageChanged(MyGUI::ComboBox* _sender, size_t pos) { onLanguageChanged(0, _sender, pos); }
            void onSecondaryLanguageChanged(MyGUI::ComboBox* _sender, size_t pos) { onLanguageChanged(1, _sender, pos); }
            void onLanguageChanged(size_t langPriority, MyGUI::ComboBox* _sender, size_t pos);

            void onWindowModeChanged(MyGUI::ComboBox* _sender, size_t pos);

            void onRebindAction(MyGUI::Widget* _sender);
            void onInputTabMouseWheel(MyGUI::Widget* _sender, int _rel);
            void onResetDefaultBindings(MyGUI::Widget* _sender);
            void onResetDefaultBindingsAccept ();
            void onKeyboardSwitchClicked(MyGUI::Widget* _sender);
            void onControllerSwitchClicked(MyGUI::Widget* _sender);

            void onWindowResize(MyGUI::Window* _sender) override;

            void onScriptFilterChange(MyGUI::EditBox*);
            void onScriptListSelection(MyGUI::ListBox*, size_t index);

            void apply();

            void configureWidgets(MyGUI::Widget* widget, bool init);
            void updateSliderLabel(MyGUI::ScrollBar* scroller, const std::string& value);

            void layoutControlsBox();
            void renderScriptSettings();

            void computeMinimumWindowSize();

        private:
            void resetScrollbars();
    };
}

#endif
