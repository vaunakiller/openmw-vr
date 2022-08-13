#include "luabindings.hpp"

#include <chrono>

#include <components/lua/luastate.hpp>
#include <components/lua/l10n.hpp>

#include "../mwbase/environment.hpp"
#include "../mwbase/statemanager.hpp"
#include "../mwworld/esmstore.hpp"
#include "../mwworld/inventorystore.hpp"
#include "../mwworld/store.hpp"

#include "eventqueue.hpp"
#include "worldview.hpp"
#include "luamanagerimp.hpp"
#include "types/types.hpp"

namespace MWLua
{

    static void addTimeBindings(sol::table& api, const Context& context, bool global)
    {
        MWBase::World* world = MWBase::Environment::get().getWorld();

        api["getSimulationTime"] = [world=context.mWorldView]() { return world->getSimulationTime(); };
        api["getSimulationTimeScale"] = [world]() { return world->getSimulationTimeScale(); };
        api["getGameTime"] = [world=context.mWorldView]() { return world->getGameTime(); };
        api["getGameTimeScale"] = [world=context.mWorldView]() { return world->getGameTimeScale(); };
        api["isWorldPaused"] = [world=context.mWorldView]() { return world->isPaused(); };
        api["getRealTime"] = []()
        {
            return std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count();
        };

        if (!global)
            return;

        api["setGameTimeScale"] = [world=context.mWorldView](double scale) { world->setGameTimeScale(scale); };

        api["setSimulationTimeScale"] = [context, world](float scale)
        {
            context.mLuaManager->addAction([scale, world] {
                world->setSimulationTimeScale(scale);
            });
        };

        // TODO: Ability to pause/resume world from Lua (needed for UI dehardcoding)
        // api["pause"] = []() {};
        // api["resume"] = []() {};
    }

    sol::table initCorePackage(const Context& context)
    {
        auto* lua = context.mLua;
        sol::table api(lua->sol(), sol::create);
        api["API_REVISION"] = 29;
        api["quit"] = [lua]()
        {
            Log(Debug::Warning) << "Quit requested by a Lua script.\n" << lua->debugTraceback();
            MWBase::Environment::get().getStateManager()->requestQuit();
        };
        api["sendGlobalEvent"] = [context](std::string eventName, const sol::object& eventData)
        {
            context.mGlobalEventQueue->push_back({std::move(eventName), LuaUtil::serialize(eventData, context.mSerializer)});
        };
        addTimeBindings(api, context, false);
        api["l10n"] = [l10n=context.mL10n](const std::string& context, const sol::object &fallbackLocale) {
            if (fallbackLocale == sol::nil)
                return l10n->getContext(context);
            else
                return l10n->getContext(context, fallbackLocale.as<std::string>());
        };
        const MWWorld::Store<ESM::GameSetting>* gmst = &MWBase::Environment::get().getWorld()->getStore().get<ESM::GameSetting>();
        api["getGMST"] = [lua=context.mLua, gmst](const std::string& setting) -> sol::object
        {
            const ESM::Variant& value = gmst->find(setting)->mValue;
            if (value.getType() == ESM::VT_String)
                return sol::make_object<std::string>(lua->sol(), value.getString());
            else if (value.getType() == ESM::VT_Int)
                return sol::make_object<int>(lua->sol(), value.getInteger());
            else
                return sol::make_object<float>(lua->sol(), value.getFloat());
        };
        return LuaUtil::makeReadOnly(api);
    }

    sol::table initWorldPackage(const Context& context)
    {
        sol::table api(context.mLua->sol(), sol::create);
        WorldView* worldView = context.mWorldView;
        addTimeBindings(api, context, true);
        api["getCellByName"] = [worldView=context.mWorldView](const std::string& name) -> sol::optional<GCell>
        {
            MWWorld::CellStore* cell = worldView->findNamedCell(name);
            if (cell)
                return GCell{cell};
            else
                return sol::nullopt;
        };
        api["getExteriorCell"] = [worldView=context.mWorldView](int x, int y) -> sol::optional<GCell>
        {
            MWWorld::CellStore* cell = worldView->findExteriorCell(x, y);
            if (cell)
                return GCell{cell};
            else
                return sol::nullopt;
        };
        api["activeActors"] = GObjectList{worldView->getActorsInScene()};
        // TODO: add world.placeNewObject(recordId, cell, pos, [rot])
        return LuaUtil::makeReadOnly(api);
    }

    sol::table initGlobalStoragePackage(const Context& context, LuaUtil::LuaStorage* globalStorage)
    {
        sol::table res(context.mLua->sol(), sol::create);
        res["globalSection"] = [globalStorage](std::string_view section) { return globalStorage->getMutableSection(section); };
        res["allGlobalSections"] = [globalStorage]() { return globalStorage->getAllSections(); };
        return LuaUtil::makeReadOnly(res);
    }

    sol::table initLocalStoragePackage(const Context& context, LuaUtil::LuaStorage* globalStorage)
    {
        sol::table res(context.mLua->sol(), sol::create);
        res["globalSection"] = [globalStorage](std::string_view section) { return globalStorage->getReadOnlySection(section); };
        return LuaUtil::makeReadOnly(res);
    }

    sol::table initPlayerStoragePackage(const Context& context, LuaUtil::LuaStorage* globalStorage, LuaUtil::LuaStorage* playerStorage)
    {
        sol::table res(context.mLua->sol(), sol::create);
        res["globalSection"] = [globalStorage](std::string_view section) { return globalStorage->getReadOnlySection(section); };
        res["playerSection"] = [playerStorage](std::string_view section) { return playerStorage->getMutableSection(section); };
        res["allPlayerSections"] = [playerStorage]() { return playerStorage->getAllSections(); };
        return LuaUtil::makeReadOnly(res);
    }

}

