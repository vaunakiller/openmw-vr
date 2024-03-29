#ifndef MWGUI_ALCHEMY_H
#define MWGUI_ALCHEMY_H

#include <memory>
#include <vector>

#include <MyGUI_ControllerItem.h>
#include <MyGUI_ComboBox.h>
#include <MyGUI_ListBox.h>

#include <components/widgets/box.hpp>
#include <components/widgets/numericeditbox.hpp>

#include "windowbase.hpp"

namespace MWMechanics
{
    class Alchemy;
}

namespace MWVR
{
    class VrListBox;
}

namespace MWGui
{
    class ItemView;
    class ItemWidget;
    class InventoryItemModel;
    class SortFilterItemModel;

    class AlchemyWindow : public WindowBase
    {
    public:
        AlchemyWindow();

        void onOpen() override;

        void onResChange(int, int) override { center(); }

    private:

        static const float sCountChangeInitialPause; // in seconds
        static const float sCountChangeInterval; // in seconds

        std::string mSuggestedPotionName;
        enum class FilterType { ByName, ByEffect };
        FilterType mCurrentFilter;

        ItemView* mItemView;
        InventoryItemModel* mModel;
        SortFilterItemModel* mSortModel;

        MyGUI::Button* mCreateButton;
        MyGUI::Button* mCancelButton;

        MyGUI::Widget* mEffectsBox;

        MyGUI::Button* mIncreaseButton;
        MyGUI::Button* mDecreaseButton;
        Gui::AutoSizedButton* mFilterType;
        MyGUI::ComboBox* mFilterCombo;
        MyGUI::EditBox* mFilterEdit;
        MyGUI::Button* mFilterButton;
#ifdef USE_OPENXR
        MWVR::VrListBox* mFilterListBox = nullptr;
#endif
        MyGUI::EditBox* mNameEdit;
        Gui::NumericEditBox* mBrewCountEdit;

        std::set<std::string> mItemNames;
        std::set<std::string> mItemEffects;

        void onCancelButtonClicked(MyGUI::Widget* _sender);
        void onCreateButtonClicked(MyGUI::Widget* _sender);
        void onIngredientSelected(MyGUI::Widget* _sender);
        void onAccept(MyGUI::EditBox*);
        void onIncreaseButtonPressed(MyGUI::Widget* _sender, int _left, int _top, MyGUI::MouseButton _id);
        void onDecreaseButtonPressed(MyGUI::Widget* _sender, int _left, int _top, MyGUI::MouseButton _id);
        void onCountButtonReleased(MyGUI::Widget* _sender, int _left, int _top, MyGUI::MouseButton _id);
        void onCountValueChanged(int value);
        void onRepeatClick(MyGUI::Widget* widget, MyGUI::ControllerItem* controller);

        void applyFilter(const std::string& filter);
        void initFilter();
        void onFilterChanged(MyGUI::ComboBox* _sender, size_t _index);
        void onFilterEdited(MyGUI::EditBox* _sender);
        void onFilterButtonClicked(MyGUI::Widget* _sender);
        void switchFilterType(MyGUI::Widget* _sender);
        void updateFilters();
        const std::set<std::string>& items();

        void addRepeatController(MyGUI::Widget* widget);

        void onIncreaseButtonTriggered();
        void onDecreaseButtonTriggered();

        void onSelectedItem(int index);

        void removeIngredient(MyGUI::Widget* ingredient);

        void createPotions(int count);

        void update();

        std::unique_ptr<MWMechanics::Alchemy> mAlchemy;

        std::vector<ItemWidget*> mApparatus;
        std::vector<ItemWidget*> mIngredients;
    };
}

#endif
