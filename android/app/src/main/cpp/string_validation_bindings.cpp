#include "string_validation_bindings.h"

#include <filesystem>
#include <fstream>
#include <mutex>
#include <string>
#include <vector>

#include "startup_contract.h"

#if defined(NEVERGONE_HAS_LUA)
extern "C" {
#include <lauxlib.h>
#include <lua.h>
}
#endif

namespace nevergone::lua_runtime {

#if defined(NEVERGONE_HAS_LUA)
namespace {

std::mutex g_dictionary_mutex;
std::string g_dictionary_path;
std::vector<std::string> g_dictionary;

void load_dictionary_locked(const std::string& path) {
    if (g_dictionary_path == path) {
        return;
    }

    g_dictionary_path = path;
    g_dictionary.clear();

    std::ifstream input(path, std::ios::binary);
    if (!input) {
        return;
    }

    std::string line;
    bool first_line = true;
    while (std::getline(input, line)) {
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        if (first_line) {
            first_line = false;
            if (line.size() >= 3 &&
                static_cast<unsigned char>(line[0]) == 0xef &&
                static_cast<unsigned char>(line[1]) == 0xbb &&
                static_cast<unsigned char>(line[2]) == 0xbf) {
                line.erase(0, 3);
            }
        }
        if (!line.empty()) {
            g_dictionary.push_back(line);
        }
    }
}

bool string_is_legal(const std::string& value) {
    if (value.empty()) {
        return false;
    }

    const std::filesystem::path dictionary_path =
        std::filesystem::path(startup::config().files_dir) / "assets" / "newWord.txt";

    std::lock_guard<std::mutex> lock(g_dictionary_mutex);
    load_dictionary_locked(dictionary_path.string());

    // The original LGG_FilterKeyWords::isLegal() returns false when its full
    // dictionary segmentation produces at least one matched word. For the
    // boolean Lua contract, searching each dictionary entry as a UTF-8 byte
    // substring preserves that observable result without reproducing the
    // original dictionary implementation itself.
    for (const std::string& blocked : g_dictionary) {
        if (value.find(blocked) != std::string::npos) {
            return false;
        }
    }
    return true;
}

int l_Lua_CheckStringLegal(lua_State* state) {
    size_t length = 0;
    const char* value = luaL_checklstring(state, 1, &length);
    lua_pushboolean(state, string_is_legal(std::string(value, length)) ? 1 : 0);
    return 1;
}

}  // namespace
#endif

void register_string_validation_bindings(lua_State* state) {
#if defined(NEVERGONE_HAS_LUA)
    lua_pushcfunction(state, l_Lua_CheckStringLegal);
    lua_setglobal(state, "Lua_CheckStringLegal");
#else
    (void)state;
#endif
}

}  // namespace nevergone::lua_runtime
