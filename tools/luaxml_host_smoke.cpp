#include <filesystem>
#include <iostream>
#include <string>

#include "luaxml_compat.h"

extern "C" {
#include <lauxlib.h>
#include <lua.h>
#include <lualib.h>
}

int main() {
    lua_State* state = luaL_newstate();
    if (state == nullptr) {
        std::cerr << "luaL_newstate failed\n";
        return 1;
    }
    luaL_openlibs(state);
    nevergone::lua_runtime::register_luaxml_compat(state);

    const std::filesystem::path smoke_path =
        std::filesystem::temp_directory_path() / "nevergone-luaxml-smoke.xml";
    lua_pushlstring(state, smoke_path.string().data(), smoke_path.string().size());
    lua_setglobal(state, "smoke_path");

    constexpr const char* kSmoke = R"LUA(
assert(type(xml) == "table")
assert(type(xml.load) == "function")
assert(type(xml.loadString) == "function")
assert(type(xml.encode) == "function")
assert(type(xml._save) == "function")

local root, parse_error = xml.loadString([[
<?xml version="1.0"?>
<!-- smoke document -->
<root id="42" label='A &amp; B'>
  <child>hello &amp; bye</child>
  <empty />
  <raw><![CDATA[<x>&raw</x>]]></raw>
  <?inside ignored?>
</root>
]])
assert(root ~= nil, parse_error)
assert(root[0] == "root")
assert(root.id == "42")
assert(root.label == "A & B")
assert(root[1][0] == "child" and root[1][1] == "hello & bye")
assert(root[2][0] == "empty")
assert(root[3][0] == "raw" and root[3][1] == "<x>&raw</x>")
assert(getmetatable(root).__index == xml)
assert(getmetatable(root[1]).__index == xml)

-- Game.Engine.xml installs methods after the native table already exists.
-- Parsed nodes must observe those later additions through __index=xml.
xml.find = function(self, tag)
    if self[0] == tag then return self end
    for _, child in ipairs(self) do
        if type(child) == "table" then
            local found = child:find(tag)
            if found ~= nil then return found end
        end
    end
end
assert(root:find("child") == root[1])
assert(root:find("empty") == root[2])

assert(xml.encode([[<&"']]) == "&lt;&amp;&quot;&apos;")
local numeric = assert(xml.loadString([[<n>&#1053;&#x435;&#1074;&#1077;&#1088;</n>]]))
assert(numeric[1] == "Невер")

local saved_ok, save_error = xml._save([[<saved value="yes"><v>7</v></saved>]], smoke_path)
assert(saved_ok == true, save_error)
local saved, load_error = xml.load(smoke_path)
assert(saved ~= nil, load_error)
assert(saved[0] == "saved" and saved.value == "yes")
assert(saved[1][0] == "v" and saved[1][1] == "7")

local invalid = xml.loadString([[<a><b></a>]])
assert(invalid == nil)
print("LuaXML compatibility smoke: ok")
)LUA";

    int status = luaL_loadstring(state, kSmoke);
    if (status == 0) {
        status = lua_pcall(state, 0, 0, 0);
    }

    if (status != 0) {
        const char* error = lua_tostring(state, -1);
        std::cerr << "LuaXML compatibility smoke failed";
        if (error != nullptr) {
            std::cerr << ": " << error;
        }
        std::cerr << "\n";
        lua_close(state);
        std::error_code ec;
        std::filesystem::remove(smoke_path, ec);
        return 1;
    }

    lua_close(state);
    std::error_code ec;
    std::filesystem::remove(smoke_path, ec);
    std::cout << "LuaXML host smoke: ok\n";
    return 0;
}
