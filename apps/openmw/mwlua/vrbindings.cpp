#include "vrbindings.hpp"

#include <components/lua/luastate.hpp>
#include <components/lua/utilpackage.hpp>
#include <components/vr/vr.hpp>

namespace MWLua
{

    sol::table initVRPackage(const Context& context)
    {
        sol::table api(context.mLua->sol(), sol::create);

        api["isVr"] = []() -> bool { return VR::getVR(); };
        return LuaUtil::makeReadOnly(api);
    }

}
