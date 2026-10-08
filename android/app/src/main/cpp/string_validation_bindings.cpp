#include "string_validation_bindings.h"

#include <cstdint>
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

bool decode_utf8_codepoint(const std::string& value, size_t* offset, std::uint32_t* codepoint) {
    if (*offset >= value.size()) {
        return false;
    }

    const auto byte_at = [&](size_t index) {
        return static_cast<unsigned char>(value[index]);
    };

    const unsigned char first = byte_at(*offset);
    if (first <= 0x7f) {
        *codepoint = first;
        ++(*offset);
        return true;
    }

    size_t width = 0;
    std::uint32_t result = 0;
    std::uint32_t minimum = 0;
    if ((first & 0xe0) == 0xc0) {
        width = 2;
        result = first & 0x1f;
        minimum = 0x80;
    } else if ((first & 0xf0) == 0xe0) {
        width = 3;
        result = first & 0x0f;
        minimum = 0x800;
    } else if ((first & 0xf8) == 0xf0) {
        width = 4;
        result = first & 0x07;
        minimum = 0x10000;
    } else {
        return false;
    }

    if (*offset + width > value.size()) {
        return false;
    }
    for (size_t index = 1; index < width; ++index) {
        const unsigned char next = byte_at(*offset + index);
        if ((next & 0xc0) != 0x80) {
            return false;
        }
        result = (result << 6) | (next & 0x3f);
    }

    if (result < minimum || result > 0x10ffff ||
        (result >= 0xd800 && result <= 0xdfff)) {
        return false;
    }

    *offset += width;
    *codepoint = result;
    return true;
}

bool is_han_codepoint(std::uint32_t codepoint) {
    return (codepoint >= 0x3400 && codepoint <= 0x4dbf) ||
           (codepoint >= 0x4e00 && codepoint <= 0x9fff) ||
           (codepoint >= 0xf900 && codepoint <= 0xfaff) ||
           (codepoint >= 0x20000 && codepoint <= 0x2ebef) ||
           (codepoint >= 0x30000 && codepoint <= 0x323af);
}

bool nickname_is_valid(const std::string& value) {
    if (value.empty()) {
        return false;
    }

    size_t offset = 0;
    while (offset < value.size()) {
        std::uint32_t codepoint = 0;
        if (!decode_utf8_codepoint(value, &offset, &codepoint)) {
            return false;
        }

        const bool ascii_letter =
            (codepoint >= 'A' && codepoint <= 'Z') ||
            (codepoint >= 'a' && codepoint <= 'z');
        const bool ascii_digit = codepoint >= '0' && codepoint <= '9';
        if (!ascii_letter && !ascii_digit && !is_han_codepoint(codepoint)) {
            return false;
        }
    }
    return true;
}

int l_Lua_CheckNickName(lua_State* state) {
    size_t length = 0;
    const char* value = luaL_checklstring(state, 1, &length);
    lua_pushboolean(state, nickname_is_valid(std::string(value, length)) ? 1 : 0);
    return 1;
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
    lua_pushcfunction(state, l_Lua_CheckNickName);
    lua_setglobal(state, "Lua_CheckNickName");

    lua_pushcfunction(state, l_Lua_CheckStringLegal);
    lua_setglobal(state, "Lua_CheckStringLegal");
#else
    (void)state;
#endif
}

}  // namespace nevergone::lua_runtime
