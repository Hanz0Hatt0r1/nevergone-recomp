#include "luaxml_compat.h"

#include <cctype>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>

#if defined(NEVERGONE_HAS_LUA)
extern "C" {
#include <lauxlib.h>
#include <lua.h>
}
#endif

namespace nevergone::lua_runtime {

#if defined(NEVERGONE_HAS_LUA)
namespace {

std::string decode_entities(std::string_view value) {
    std::string out;
    out.reserve(value.size());
    for (size_t i = 0; i < value.size(); ++i) {
        if (value[i] != '&') {
            out.push_back(value[i]);
            continue;
        }
        const size_t end = value.find(';', i + 1);
        if (end == std::string_view::npos) {
            out.push_back('&');
            continue;
        }
        const std::string entity(value.substr(i + 1, end - i - 1));
        if (entity == "amp") out.push_back('&');
        else if (entity == "lt") out.push_back('<');
        else if (entity == "gt") out.push_back('>');
        else if (entity == "quot") out.push_back('"');
        else if (entity == "apos") out.push_back('\'');
        else if (!entity.empty() && entity[0] == '#') {
            try {
                unsigned long cp = 0;
                if (entity.size() > 2 && (entity[1] == 'x' || entity[1] == 'X')) {
                    cp = std::stoul(entity.substr(2), nullptr, 16);
                } else {
                    cp = std::stoul(entity.substr(1), nullptr, 10);
                }
                if (cp <= 0x7f) out.push_back(static_cast<char>(cp));
                else if (cp <= 0x7ff) {
                    out.push_back(static_cast<char>(0xc0 | (cp >> 6)));
                    out.push_back(static_cast<char>(0x80 | (cp & 0x3f)));
                } else if (cp <= 0xffff) {
                    out.push_back(static_cast<char>(0xe0 | (cp >> 12)));
                    out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3f)));
                    out.push_back(static_cast<char>(0x80 | (cp & 0x3f)));
                } else if (cp <= 0x10ffff) {
                    out.push_back(static_cast<char>(0xf0 | (cp >> 18)));
                    out.push_back(static_cast<char>(0x80 | ((cp >> 12) & 0x3f)));
                    out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3f)));
                    out.push_back(static_cast<char>(0x80 | (cp & 0x3f)));
                }
            } catch (...) {
                out.append(value.substr(i, end - i + 1));
            }
        } else {
            out.append(value.substr(i, end - i + 1));
        }
        i = end;
    }
    return out;
}

std::string encode_entities(std::string_view value) {
    std::string out;
    out.reserve(value.size());
    for (char ch : value) {
        switch (ch) {
            case '&': out += "&amp;"; break;
            case '<': out += "&lt;"; break;
            case '>': out += "&gt;"; break;
            case '"': out += "&quot;"; break;
            case '\'': out += "&apos;"; break;
            default: out.push_back(ch); break;
        }
    }
    return out;
}

class Parser {
public:
    Parser(lua_State* state, std::string_view text) : state_(state), text_(text) {}

    void parse_document() {
        skip_misc();
        parse_element();
        skip_misc();
        skip_space();
        if (position_ != text_.size()) fail("trailing XML data");
    }

private:
    lua_State* state_;
    std::string_view text_;
    size_t position_ = 0;

    [[noreturn]] void fail(const char* message) const {
        throw std::runtime_error(std::string(message) + " at byte " + std::to_string(position_));
    }

    bool starts_with(std::string_view value) const {
        return text_.substr(position_, value.size()) == value;
    }

    void skip_space() {
        while (position_ < text_.size() && std::isspace(static_cast<unsigned char>(text_[position_]))) {
            ++position_;
        }
    }

    void skip_until(std::string_view marker) {
        const size_t end = text_.find(marker, position_);
        if (end == std::string_view::npos) fail("unterminated XML section");
        position_ = end + marker.size();
    }

    void skip_misc() {
        while (true) {
            skip_space();
            if (starts_with("<?")) {
                position_ += 2;
                skip_until("?>");
            } else if (starts_with("<!--")) {
                position_ += 4;
                skip_until("-->");
            } else if (starts_with("<!DOCTYPE")) {
                position_ += 9;
                int bracket_depth = 0;
                while (position_ < text_.size()) {
                    const char ch = text_[position_++];
                    if (ch == '[') ++bracket_depth;
                    else if (ch == ']') --bracket_depth;
                    else if (ch == '>' && bracket_depth <= 0) break;
                }
            } else {
                break;
            }
        }
    }

    std::string parse_name() {
        const size_t start = position_;
        while (position_ < text_.size()) {
            const char ch = text_[position_];
            if (std::isalnum(static_cast<unsigned char>(ch)) || ch == '_' || ch == '-' || ch == ':' || ch == '.') {
                ++position_;
            } else {
                break;
            }
        }
        if (position_ == start) fail("expected XML name");
        return std::string(text_.substr(start, position_ - start));
    }

    std::string parse_quoted() {
        if (position_ >= text_.size() || (text_[position_] != '"' && text_[position_] != '\'')) {
            fail("expected quoted attribute value");
        }
        const char quote = text_[position_++];
        const size_t start = position_;
        while (position_ < text_.size() && text_[position_] != quote) ++position_;
        if (position_ >= text_.size()) fail("unterminated attribute value");
        std::string value = decode_entities(text_.substr(start, position_ - start));
        ++position_;
        return value;
    }

    void append_string(int table_index, int& array_index, std::string value) {
        if (value.empty()) return;
        bool non_space = false;
        for (char ch : value) {
            if (!std::isspace(static_cast<unsigned char>(ch))) {
                non_space = true;
                break;
            }
        }
        if (!non_space) return;
        lua_pushlstring(state_, value.data(), value.size());
        lua_rawseti(state_, table_index, array_index++);
    }

    void parse_element() {
        if (position_ >= text_.size() || text_[position_] != '<') fail("expected element");
        ++position_;
        if (position_ < text_.size() && text_[position_] == '/') fail("unexpected closing tag");

        const std::string tag = parse_name();
        lua_newtable(state_);
        const int table_index = lua_gettop(state_);
        lua_pushlstring(state_, tag.data(), tag.size());
        lua_rawseti(state_, table_index, 0);

        while (true) {
            skip_space();
            if (starts_with("/>")) {
                position_ += 2;
                return;
            }
            if (position_ < text_.size() && text_[position_] == '>') {
                ++position_;
                break;
            }
            const std::string key = parse_name();
            skip_space();
            if (position_ >= text_.size() || text_[position_] != '=') fail("expected '=' after attribute");
            ++position_;
            skip_space();
            const std::string value = parse_quoted();
            lua_pushlstring(state_, value.data(), value.size());
            lua_setfield(state_, table_index, key.c_str());
        }

        int array_index = 1;
        while (position_ < text_.size()) {
            if (starts_with("</")) {
                position_ += 2;
                const std::string closing = parse_name();
                if (closing != tag) fail("mismatched closing tag");
                skip_space();
                if (position_ >= text_.size() || text_[position_] != '>') fail("expected closing '>'");
                ++position_;
                return;
            }
            if (starts_with("<!--")) {
                position_ += 4;
                skip_until("-->");
                continue;
            }
            if (starts_with("<![CDATA[")) {
                position_ += 9;
                const size_t end = text_.find("]]>", position_);
                if (end == std::string_view::npos) fail("unterminated CDATA");
                append_string(table_index, array_index, std::string(text_.substr(position_, end - position_)));
                position_ = end + 3;
                continue;
            }
            if (position_ < text_.size() && text_[position_] == '<') {
                parse_element();
                lua_rawseti(state_, table_index, array_index++);
                continue;
            }
            const size_t start = position_;
            while (position_ < text_.size() && text_[position_] != '<') ++position_;
            append_string(table_index, array_index, decode_entities(text_.substr(start, position_ - start)));
        }
        fail("unterminated element");
    }
};

int l_xml_encode(lua_State* state) {
    size_t length = 0;
    const char* input = luaL_checklstring(state, 1, &length);
    const std::string escaped = encode_entities(std::string_view(input, length));
    lua_pushlstring(state, escaped.data(), escaped.size());
    return 1;
}

int l_xml_load(lua_State* state) {
    const char* filename = luaL_checkstring(state, 1);
    std::ifstream input(filename, std::ios::binary);
    if (!input) {
        lua_pushnil(state);
        return 1;
    }
    std::ostringstream buffer;
    buffer << input.rdbuf();
    const std::string data = buffer.str();
    try {
        Parser parser(state, data);
        parser.parse_document();
        return 1;
    } catch (const std::exception& error) {
        lua_settop(state, 0);
        lua_pushnil(state);
        lua_pushstring(state, error.what());
        return 2;
    }
}

int l_xml_save(lua_State* state) {
    size_t length = 0;
    const char* content = luaL_checklstring(state, 1, &length);
    const char* filename = luaL_checkstring(state, 2);
    try {
        const std::filesystem::path target(filename);
        if (!target.parent_path().empty()) {
            std::filesystem::create_directories(target.parent_path());
        }
        const std::filesystem::path temporary = target.string() + ".tmp";
        {
            std::ofstream output(temporary, std::ios::binary | std::ios::trunc);
            if (!output) throw std::runtime_error("cannot open XML output");
            output.write(content, static_cast<std::streamsize>(length));
            if (!output) throw std::runtime_error("cannot write XML output");
        }
        std::error_code ec;
        std::filesystem::rename(temporary, target, ec);
        if (ec) {
            std::filesystem::remove(target, ec);
            ec.clear();
            std::filesystem::rename(temporary, target, ec);
        }
        if (ec) throw std::runtime_error("cannot activate XML output");
        lua_pushboolean(state, 1);
        return 1;
    } catch (const std::exception& error) {
        lua_pushboolean(state, 0);
        lua_pushstring(state, error.what());
        return 2;
    }
}

}  // namespace
#endif

void register_luaxml_compat(lua_State* state) {
#if defined(NEVERGONE_HAS_LUA)
    lua_getglobal(state, "xml");
    if (!lua_istable(state, -1)) {
        lua_pop(state, 1);
        lua_newtable(state);
    }

    lua_pushcfunction(state, l_xml_load);
    lua_setfield(state, -2, "load");
    lua_pushcfunction(state, l_xml_encode);
    lua_setfield(state, -2, "encode");
    lua_pushcfunction(state, l_xml_save);
    lua_setfield(state, -2, "_save");

    lua_pushvalue(state, -1);
    lua_setglobal(state, "xml");

    lua_getglobal(state, "package");
    if (lua_istable(state, -1)) {
        lua_getfield(state, -1, "loaded");
        if (lua_istable(state, -1)) {
            lua_pushvalue(state, -3);
            lua_setfield(state, -2, "xml");
        }
        lua_pop(state, 1);
    }
    lua_pop(state, 2);
#else
    (void)state;
#endif
}

}  // namespace nevergone::lua_runtime
