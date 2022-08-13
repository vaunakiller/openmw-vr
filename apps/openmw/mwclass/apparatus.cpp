#include "apparatus.hpp"

#include <MyGUI_TextIterator.h>

#include <components/esm3/loadappa.hpp>

#include "../mwbase/environment.hpp"
#include "../mwbase/windowmanager.hpp"

#include "../mwworld/ptr.hpp"
#include "../mwworld/actionalchemy.hpp"
#include "../mwworld/cellstore.hpp"
#include "../mwphysics/physicssystem.hpp"
#include "../mwworld/nullaction.hpp"

#include "../mwrender/objects.hpp"
#include "../mwrender/renderinginterface.hpp"

#include "../mwgui/tooltips.hpp"

#include "classmodel.hpp"

namespace MWClass
{
    Apparatus::Apparatus()
        : MWWorld::RegisteredClass<Apparatus>(ESM::Apparatus::sRecordId)
    {
    }

    void Apparatus::insertObjectRendering (const MWWorld::Ptr& ptr, const std::string& model, MWRender::RenderingInterface& renderingInterface) const
    {
        if (!model.empty()) {
            renderingInterface.getObjects().insertModel(ptr, model);
        }
    }

    std::string Apparatus::getModel(const MWWorld::ConstPtr &ptr) const
    {
        return getClassModel<ESM::Apparatus>(ptr);
    }

    std::string Apparatus::getName (const MWWorld::ConstPtr& ptr) const
    {
        const MWWorld::LiveCellRef<ESM::Apparatus> *ref = ptr.get<ESM::Apparatus>();
        const std::string& name = ref->mBase->mName;

        return !name.empty() ? name : ref->mBase->mId;
    }

    std::unique_ptr<MWWorld::Action> Apparatus::activate (const MWWorld::Ptr& ptr,
        const MWWorld::Ptr& actor) const
    {
        return defaultItemActivate(ptr, actor);
    }

    std::string Apparatus::getScript (const MWWorld::ConstPtr& ptr) const
    {
        const MWWorld::LiveCellRef<ESM::Apparatus> *ref = ptr.get<ESM::Apparatus>();

        return ref->mBase->mScript;
    }

    int Apparatus::getValue (const MWWorld::ConstPtr& ptr) const
    {
        const MWWorld::LiveCellRef<ESM::Apparatus> *ref = ptr.get<ESM::Apparatus>();

        return ref->mBase->mData.mValue;
    }

    std::string Apparatus::getUpSoundId (const MWWorld::ConstPtr& ptr) const
    {
        return std::string("Item Apparatus Up");
    }

    std::string Apparatus::getDownSoundId (const MWWorld::ConstPtr& ptr) const
    {
        return std::string("Item Apparatus Down");
    }

    std::string Apparatus::getInventoryIcon (const MWWorld::ConstPtr& ptr) const
    {
        const MWWorld::LiveCellRef<ESM::Apparatus> *ref = ptr.get<ESM::Apparatus>();

        return ref->mBase->mIcon;
    }

    MWGui::ToolTipInfo Apparatus::getToolTipInfo (const MWWorld::ConstPtr& ptr, int count) const
    {
        const MWWorld::LiveCellRef<ESM::Apparatus> *ref = ptr.get<ESM::Apparatus>();

        MWGui::ToolTipInfo info;
        info.caption = MyGUI::TextIterator::toTagsString(getName(ptr)) + MWGui::ToolTips::getCountString(count);
        info.icon = ref->mBase->mIcon;

        std::string text;
        text += "\n#{sQuality}: " + MWGui::ToolTips::toString(ref->mBase->mData.mQuality);
        text += MWGui::ToolTips::getWeightString(ref->mBase->mData.mWeight, "#{sWeight}");
        text += MWGui::ToolTips::getValueString(ref->mBase->mData.mValue, "#{sValue}");

        if (MWBase::Environment::get().getWindowManager()->getFullHelp()) {
            text += MWGui::ToolTips::getCellRefString(ptr.getCellRef());
            text += MWGui::ToolTips::getMiscString(ref->mBase->mScript, "Script");
        }
        info.text = text;

        return info;
    }

    std::unique_ptr<MWWorld::Action> Apparatus::use (const MWWorld::Ptr& ptr, bool force) const
    {
        return std::make_unique<MWWorld::ActionAlchemy>(force);
    }

    MWWorld::Ptr Apparatus::copyToCellImpl(const MWWorld::ConstPtr &ptr, MWWorld::CellStore &cell) const
    {
        const MWWorld::LiveCellRef<ESM::Apparatus> *ref = ptr.get<ESM::Apparatus>();

        return MWWorld::Ptr(cell.insert(ref), &cell);
    }

    bool Apparatus::canSell (const MWWorld::ConstPtr& item, int npcServices) const
    {
        return (npcServices & ESM::NPC::Apparatus) != 0;
    }

    float Apparatus::getWeight(const MWWorld::ConstPtr &ptr) const
    {
        const MWWorld::LiveCellRef<ESM::Apparatus> *ref = ptr.get<ESM::Apparatus>();
        return ref->mBase->mData.mWeight;
    }
}
