#include "login_callback_payload.h"

#include <cerrno>
#include <cmath>
#include <cstdlib>
#include <map>
#include <string_view>
#include <utility>

namespace nevergone::login_callback_payload {
namespace {

enum class JsonType {
    kNull,
    kBool,
    kNumber,
    kString,
    kArray,
    kObject,
};

struct JsonValue {
    JsonType type = JsonType::kNull;
    bool boolean = false;
    double number = 0.0;
    std::string string;
    std::vector<JsonValue> array;
    std::map<std::string, JsonValue> object;
};

class JsonParser {
public:
    explicit JsonParser(std::string_view text) : text_(text) {}

    bool parse(JsonValue* output) {
        if (output == nullptr) return false;
        skip_space();
        if (!parse_value(output, 0)) return false;
        skip_space();
        return position_ == text_.size();
    }

private:
    static constexpr int kMaxDepth = 64;

    void skip_space() {
        while (position_ < text_.size()) {
            const char c = text_[position_];
            if (c != ' ' && c != '\t' && c != '\r' && c != '\n') break;
            ++position_;
        }
    }

    bool consume(char expected) {
        if (position_ >= text_.size() || text_[position_] != expected) return false;
        ++position_;
        return true;
    }

    bool consume_literal(std::string_view literal) {
        if (text_.substr(position_, literal.size()) != literal) return false;
        position_ += literal.size();
        return true;
    }

    static int hex_value(char c) {
        if (c >= '0' && c <= '9') return c - '0';
        if (c >= 'a' && c <= 'f') return 10 + c - 'a';
        if (c >= 'A' && c <= 'F') return 10 + c - 'A';
        return -1;
    }

    static void append_utf8(std::uint32_t codepoint, std::string* output) {
        if (output == nullptr) return;
        if (codepoint <= 0x7f) {
            output->push_back(static_cast<char>(codepoint));
        } else if (codepoint <= 0x7ff) {
            output->push_back(static_cast<char>(0xc0 | (codepoint >> 6)));
            output->push_back(static_cast<char>(0x80 | (codepoint & 0x3f)));
        } else if (codepoint <= 0xffff) {
            output->push_back(static_cast<char>(0xe0 | (codepoint >> 12)));
            output->push_back(static_cast<char>(0x80 | ((codepoint >> 6) & 0x3f)));
            output->push_back(static_cast<char>(0x80 | (codepoint & 0x3f)));
        } else if (codepoint <= 0x10ffff) {
            output->push_back(static_cast<char>(0xf0 | (codepoint >> 18)));
            output->push_back(static_cast<char>(0x80 | ((codepoint >> 12) & 0x3f)));
            output->push_back(static_cast<char>(0x80 | ((codepoint >> 6) & 0x3f)));
            output->push_back(static_cast<char>(0x80 | (codepoint & 0x3f)));
        }
    }

    bool parse_string(std::string* output) {
        if (output == nullptr || !consume('"')) return false;
        output->clear();
        while (position_ < text_.size()) {
            const char c = text_[position_++];
            if (c == '"') return true;
            if (static_cast<unsigned char>(c) < 0x20) return false;
            if (c != '\\') {
                output->push_back(c);
                continue;
            }
            if (position_ >= text_.size()) return false;
            const char escaped = text_[position_++];
            switch (escaped) {
                case '"': output->push_back('"'); break;
                case '\\': output->push_back('\\'); break;
                case '/': output->push_back('/'); break;
                case 'b': output->push_back('\b'); break;
                case 'f': output->push_back('\f'); break;
                case 'n': output->push_back('\n'); break;
                case 'r': output->push_back('\r'); break;
                case 't': output->push_back('\t'); break;
                case 'u': {
                    if (position_ + 4 > text_.size()) return false;
                    std::uint32_t codepoint = 0;
                    for (int i = 0; i < 4; ++i) {
                        const int value = hex_value(text_[position_++]);
                        if (value < 0) return false;
                        codepoint = (codepoint << 4) | static_cast<std::uint32_t>(value);
                    }
                    append_utf8(codepoint, output);
                    break;
                }
                default:
                    return false;
            }
        }
        return false;
    }

    bool parse_number(JsonValue* output) {
        const std::size_t begin = position_;
        if (position_ < text_.size() && text_[position_] == '-') ++position_;
        if (position_ >= text_.size()) return false;
        if (text_[position_] == '0') {
            ++position_;
        } else {
            if (text_[position_] < '1' || text_[position_] > '9') return false;
            while (position_ < text_.size() && text_[position_] >= '0' && text_[position_] <= '9') {
                ++position_;
            }
        }
        if (position_ < text_.size() && text_[position_] == '.') {
            ++position_;
            const std::size_t fraction_begin = position_;
            while (position_ < text_.size() && text_[position_] >= '0' && text_[position_] <= '9') {
                ++position_;
            }
            if (fraction_begin == position_) return false;
        }
        if (position_ < text_.size() && (text_[position_] == 'e' || text_[position_] == 'E')) {
            ++position_;
            if (position_ < text_.size() && (text_[position_] == '+' || text_[position_] == '-')) ++position_;
            const std::size_t exponent_begin = position_;
            while (position_ < text_.size() && text_[position_] >= '0' && text_[position_] <= '9') {
                ++position_;
            }
            if (exponent_begin == position_) return false;
        }

        const std::string token(text_.substr(begin, position_ - begin));
        char* end = nullptr;
        errno = 0;
        const double value = std::strtod(token.c_str(), &end);
        if (errno == ERANGE || end == nullptr || *end != '\0' || !std::isfinite(value)) return false;
        output->type = JsonType::kNumber;
        output->number = value;
        return true;
    }

    bool parse_array(JsonValue* output, int depth) {
        if (!consume('[')) return false;
        output->type = JsonType::kArray;
        output->array.clear();
        skip_space();
        if (consume(']')) return true;
        while (true) {
            JsonValue item;
            if (!parse_value(&item, depth + 1)) return false;
            output->array.push_back(std::move(item));
            skip_space();
            if (consume(']')) return true;
            if (!consume(',')) return false;
            skip_space();
        }
    }

    bool parse_object(JsonValue* output, int depth) {
        if (!consume('{')) return false;
        output->type = JsonType::kObject;
        output->object.clear();
        skip_space();
        if (consume('}')) return true;
        while (true) {
            std::string key;
            if (!parse_string(&key)) return false;
            skip_space();
            if (!consume(':')) return false;
            skip_space();
            JsonValue value;
            if (!parse_value(&value, depth + 1)) return false;
            output->object.insert_or_assign(std::move(key), std::move(value));
            skip_space();
            if (consume('}')) return true;
            if (!consume(',')) return false;
            skip_space();
        }
    }

    bool parse_value(JsonValue* output, int depth) {
        if (output == nullptr || depth > kMaxDepth) return false;
        skip_space();
        if (position_ >= text_.size()) return false;
        const char c = text_[position_];
        if (c == '"') {
            output->type = JsonType::kString;
            return parse_string(&output->string);
        }
        if (c == '[') return parse_array(output, depth);
        if (c == '{') return parse_object(output, depth);
        if (c == 't') {
            if (!consume_literal("true")) return false;
            output->type = JsonType::kBool;
            output->boolean = true;
            return true;
        }
        if (c == 'f') {
            if (!consume_literal("false")) return false;
            output->type = JsonType::kBool;
            output->boolean = false;
            return true;
        }
        if (c == 'n') {
            if (!consume_literal("null")) return false;
            output->type = JsonType::kNull;
            return true;
        }
        return parse_number(output);
    }

    std::string_view text_;
    std::size_t position_ = 0;
};

const JsonValue* field(const JsonValue& object, const char* name) {
    if (object.type != JsonType::kObject || name == nullptr) return nullptr;
    const auto iterator = object.object.find(name);
    return iterator == object.object.end() ? nullptr : &iterator->second;
}

bool int64_field(const JsonValue& object, const char* name, std::int64_t* output) {
    const JsonValue* value = field(object, name);
    if (value == nullptr || value->type != JsonType::kNumber || output == nullptr) return false;
    *output = static_cast<std::int64_t>(value->number);
    return true;
}

bool string_field(const JsonValue& object, const char* name, std::string* output) {
    const JsonValue* value = field(object, name);
    if (value == nullptr || value->type != JsonType::kString || output == nullptr) return false;
    *output = value->string;
    return true;
}

int role_richness(const RoleEntry& role) {
    int richness = 0;
    if (!role.character_name.empty()) ++richness;
    if (role.career != 0) ++richness;
    if (role.character_level != 0) ++richness;
    if (role.clothes_id != 0) ++richness;
    if (role.clothes_color_id != 0) ++richness;
    return richness;
}

void collect_roles(const JsonValue& value, std::map<std::int64_t, RoleEntry>* roles, int depth) {
    if (roles == nullptr || depth > 64) return;
    if (value.type == JsonType::kObject) {
        RoleEntry role;
        const bool has_id = int64_field(value, "CharacterID", &role.character_id);
        const bool has_name = string_field(value, "CharacterName", &role.character_name);
        if (has_id && has_name) {
            int64_field(value, "Career", &role.career);
            int64_field(value, "CharacterLevel", &role.character_level);
            int64_field(value, "ClothesID", &role.clothes_id);
            int64_field(value, "ClothesColorID", &role.clothes_color_id);
            const auto existing = roles->find(role.character_id);
            if (existing == roles->end()) {
                roles->emplace(role.character_id, std::move(role));
            } else if (role_richness(role) > role_richness(existing->second)) {
                existing->second = std::move(role);
            }
        }
        for (const auto& entry : value.object) collect_roles(entry.second, roles, depth + 1);
    } else if (value.type == JsonType::kArray) {
        for (const auto& item : value.array) collect_roles(item, roles, depth + 1);
    }
}

}  // namespace

bool parse_server_list_callback(
    const std::vector<std::string>& arguments,
    ServerListPayload* output) {
    if (output == nullptr) return false;
    *output = ServerListPayload{};
    if (arguments.empty()) return false;

    JsonValue root;
    JsonParser parser(arguments[0]);
    if (!parser.parse(&root) || root.type != JsonType::kArray) return false;

    for (const JsonValue& value : root.array) {
        if (value.type != JsonType::kObject) continue;
        ServerEntry server;
        const bool has_id = int64_field(value, "id", &server.id);
        const bool has_name = string_field(value, "name", &server.name);
        const bool has_ip = string_field(value, "ip", &server.ip);
        string_field(value, "BattleIP", &server.battle_ip);
        if (has_id && has_name && has_ip) output->servers.push_back(std::move(server));
    }

    if (arguments.size() >= 2) output->last_login_server = arguments[1];
    output->valid = true;
    return true;
}

bool parse_role_list_callback(
    const std::vector<std::string>& arguments,
    RoleListPayload* output) {
    if (output == nullptr) return false;
    *output = RoleListPayload{};
    if (arguments.empty()) return false;

    JsonValue root;
    JsonParser parser(arguments[0]);
    if (!parser.parse(&root)) return false;

    std::map<std::int64_t, RoleEntry> roles;
    collect_roles(root, &roles, 0);
    output->roles.reserve(roles.size());
    for (auto& entry : roles) output->roles.push_back(std::move(entry.second));
    output->valid = true;
    return true;
}

}  // namespace nevergone::login_callback_payload
