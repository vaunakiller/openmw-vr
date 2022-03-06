#include <components/lua_ui/util.hpp>
#include <components/lua_ui/element.hpp>
#include <components/lua_ui/layers.hpp>
#include <components/lua_ui/content.hpp>
#include <components/lua_ui/registerscriptsettings.hpp>
#include <components/lua_ui/alignment.hpp>
#include <components/lua_ui/resources.hpp>

#include "context.hpp"
#include "actions.hpp"
#include "luamanagerimp.hpp"

namespace MWLua
{
    namespace
    {
        class UiAction final : public Action
        {
            public:
                enum Type
                {
                    CREATE = 0,
                    UPDATE,
                    DESTROY,
                };

                UiAction(Type type, std::shared_ptr<LuaUi::Element> element, LuaUtil::LuaState* state)
                    : Action(state)
                    , mType{ type }
                    , mElement{ std::move(element) }
                {}

                void apply(WorldView&) const override
                {
                    try {
                        switch (mType)
                        {
                            case CREATE:
                                mElement->create();
                                break;
                            case UPDATE:
                                mElement->update();
                                break;
                            case DESTROY:
                                mElement->destroy();
                                break;
                        }
                    }
                    catch (std::exception&)
                    {
                        // prevent any actions on a potentially corrupted widget
                        mElement->mRoot = nullptr;
                        throw;
                    }
                }

                std::string toString() const override
                {
                    std::string result;
                    switch (mType)
                    {
                        case CREATE:
                            result += "Create";
                            break;
                        case UPDATE:
                            result += "Update";
                            break;
                        case DESTROY:
                            result += "Destroy";
                            break;
                    }
                    result += " UI";
                    return result;
                }

            private:
                Type mType;
                std::shared_ptr<LuaUi::Element> mElement;
        };

        class InsertLayerAction final : public Action
        {
            public:
                InsertLayerAction(std::string_view name, std::string_view afterName,
                    LuaUi::Layers::Options options, LuaUtil::LuaState* state)
                    : Action(state)
                    , mName(name)
                    , mAfterName(afterName)
                    , mOptions(options)
                {}

                void apply(WorldView&) const override
                {
                    size_t index = LuaUi::Layers::indexOf(mAfterName);
                    if (index == LuaUi::Layers::size())
                        throw std::logic_error(std::string("Layer not found"));
                    LuaUi::Layers::insert(index, mName, mOptions);
                }

                std::string toString() const override
                {
                    std::string result("Insert UI layer \"");
                    result += mName;
                    result += "\" after \"";
                    result += mAfterName;
                    result += "\"";
                    return result;
                }

            private:
                std::string mName;
                std::string mAfterName;
                LuaUi::Layers::Options mOptions;
        };

        // Lua arrays index from 1
        inline size_t fromLuaIndex(size_t i) { return i - 1; }
        inline size_t toLuaIndex(size_t i) { return i + 1; }
    }

    sol::table initUserInterfacePackage(const Context& context)
    {
        auto uiContent = context.mLua->sol().new_usertype<LuaUi::Content>("UiContent");
        uiContent[sol::meta_function::length] = [](const LuaUi::Content& content)
        {
            return content.size();
        };
        uiContent[sol::meta_function::index] = sol::overload(
            [](const LuaUi::Content& content, size_t index)
            {
                return content.at(fromLuaIndex(index));
            },
            [](const LuaUi::Content& content, std::string_view name)
            {
                return content.at(name);
            });
        uiContent[sol::meta_function::new_index] = sol::overload(
            [](LuaUi::Content& content, size_t index, const sol::table& table)
            {
                content.assign(fromLuaIndex(index), table);
            },
            [](LuaUi::Content& content, size_t index, sol::nil_t nil)
            {
                content.remove(fromLuaIndex(index));
            },
            [](LuaUi::Content& content, std::string_view name, const sol::table& table)
            {
                content.assign(name, table);
            },
            [](LuaUi::Content& content, std::string_view name, sol::nil_t nil)
            {
                content.remove(name);
            });
        uiContent["insert"] = [](LuaUi::Content& content, size_t index, const sol::table& table)
        {
            content.insert(fromLuaIndex(index), table);
        };
        uiContent["add"] = [](LuaUi::Content& content, const sol::table& table)
        {
            content.insert(content.size(), table);
        };
        uiContent["indexOf"] = [](LuaUi::Content& content, const sol::table& table) -> sol::optional<size_t>
        {
            size_t index = content.indexOf(table);
            if (index < content.size())
                return toLuaIndex(index);
            else
                return sol::nullopt;
        };
        {
            auto pairs = [](LuaUi::Content& content)
            {
                auto next = [](LuaUi::Content& content, size_t i) -> sol::optional<std::tuple<size_t, sol::table>>
                {
                    if (i < content.size())
                        return std::make_tuple(i + 1, content.at(i));
                    else
                        return sol::nullopt;
                };
                return std::make_tuple(next, content, 0);
            };
            uiContent[sol::meta_function::ipairs] = pairs;
            uiContent[sol::meta_function::pairs] = pairs;
        }

        auto element = context.mLua->sol().new_usertype<LuaUi::Element>("Element");
        element["layout"] = sol::property(
            [](LuaUi::Element& element)
            {
                return element.mLayout;
            },
            [](LuaUi::Element& element, const sol::table& layout)
            {
                element.mLayout = layout;
            }
        );
        element["update"] = [context](const std::shared_ptr<LuaUi::Element>& element)
        {
            if (element->mDestroy || element->mUpdate)
                return;
            element->mUpdate = true;
            context.mLuaManager->addAction(std::make_unique<UiAction>(UiAction::UPDATE, element, context.mLua));
        };
        element["destroy"] = [context](const std::shared_ptr<LuaUi::Element>& element)
        {
            if (element->mDestroy)
                return;
            element->mDestroy = true;
            context.mLuaManager->addAction(std::make_unique<UiAction>(UiAction::DESTROY, element, context.mLua));
        };

        sol::table api = context.mLua->newTable();
        api["showMessage"] = [luaManager=context.mLuaManager](std::string_view message)
        {
            luaManager->addUIMessage(message);
        };
        api["content"] = [](const sol::table& table)
        {
            return LuaUi::Content(table);
        };
        api["create"] = [context](const sol::table& layout)
        {
            auto element = LuaUi::Element::make(layout);
            context.mLuaManager->addAction(std::make_unique<UiAction>(UiAction::CREATE, element, context.mLua));
            return element;
        };

        sol::table layers = context.mLua->newTable();
        layers[sol::meta_function::length] = []()
        {
            return LuaUi::Layers::size();
        };
        layers[sol::meta_function::index] = [](size_t index)
        {
            index = fromLuaIndex(index);
            return LuaUi::Layers::at(index);
        };
        layers["indexOf"] = [](std::string_view name) -> sol::optional<size_t>
        {
            size_t index = LuaUi::Layers::indexOf(name);
            if (index == LuaUi::Layers::size())
                return sol::nullopt;
            else
                return toLuaIndex(index);
        };
        layers["insertAfter"] = [context](std::string_view afterName, std::string_view name, const sol::object& opt)
        {
            LuaUi::Layers::Options options;
            options.mInteractive = LuaUtil::getValueOrDefault(LuaUtil::getFieldOrNil(opt, "interactive"), true);
            context.mLuaManager->addAction(std::make_unique<InsertLayerAction>(name, afterName, options, context.mLua));
        };
        {
            auto pairs = [layers](const sol::object&)
            {
                auto next = [](const sol::table& l, size_t i) -> sol::optional<std::tuple<size_t, std::string>>
                {
                    if (i < LuaUi::Layers::size())
                        return std::make_tuple(i + 1, LuaUi::Layers::at(i));
                    else
                        return sol::nullopt;
                };
                return std::make_tuple(next, layers, 0);
            };
            layers[sol::meta_function::pairs] = pairs;
            layers[sol::meta_function::ipairs] = pairs;
        }
        api["layers"] = LuaUtil::makeReadOnly(layers);

        sol::table typeTable = context.mLua->newTable();
        for (const auto& it : LuaUi::widgetTypeToName())
            typeTable.set(it.second, it.first);
        api["TYPE"] = LuaUtil::makeReadOnly(typeTable);

        api["ALIGNMENT"] = LuaUtil::makeReadOnly(context.mLua->tableFromPairs<std::string_view, LuaUi::Alignment>({
            { "Start", LuaUi::Alignment::Start },
            { "Center", LuaUi::Alignment::Center },
            { "End", LuaUi::Alignment::End }
        }));

        api["registerSettingsPage"] = &LuaUi::registerSettingsPage;

        api["texture"] = [luaManager=context.mLuaManager](const sol::table& options)
        {
            LuaUi::TextureData data;
            sol::object path = LuaUtil::getFieldOrNil(options, "path");
            if (path.is<std::string>())
                data.mPath = path.as<std::string>();
            if (data.mPath.empty())
                throw std::logic_error("Invalid texture path");
            sol::object offset = LuaUtil::getFieldOrNil(options, "offset");
            if (offset.is<osg::Vec2f>())
                data.mOffset = offset.as<osg::Vec2f>();
            sol::object size = LuaUtil::getFieldOrNil(options, "size");
            if (size.is<osg::Vec2f>())
                data.mSize = size.as<osg::Vec2f>();
            return luaManager->uiResourceManager()->registerTexture(data);
        };

        return LuaUtil::makeReadOnly(api);
    }
}
