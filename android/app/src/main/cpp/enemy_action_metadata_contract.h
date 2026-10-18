#pragma once

#include <string_view>

namespace nevergone::enemy_action_metadata_contract {

// cocos2d::CCString::boolValue() returns false for an empty value, "false",
// or "0"; other non-empty strings are true. A missing dictionary key resolves
// to an empty CCString in the recovered loader path.
constexpr bool native_bool_value(std::string_view value) noexcept {
    return !value.empty() && value != "false" && value != "0";
}

struct Flags {
    bool not_hit = native_bool_value("");
    bool go = native_bool_value("");
};

}  // namespace nevergone::enemy_action_metadata_contract
