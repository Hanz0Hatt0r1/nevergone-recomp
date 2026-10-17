#include "game_scene_player_bootstrap.h"

#include <cmath>
#include <cstdio>
#include <limits>

#include "character_name_state.h"
#include "role_selection_state.h"

namespace nevergone::game_scene_player_bootstrap {
namespace {

std::string format_name(const char* format, std::uint32_t a) {
    char buffer[64] = {};
    const int written = std::snprintf(buffer, sizeof(buffer), format, a);
    if (written <= 0 || static_cast<std::size_t>(written) >= sizeof(buffer)) return {};
    return std::string(buffer, static_cast<std::size_t>(written));
}

std::string format_name(
        const char* format,
        std::uint32_t a,
        std::uint32_t b,
        std::uint32_t c) {
    char buffer[64] = {};
    const int written = std::snprintf(buffer, sizeof(buffer), format, a, b, c);
    if (written <= 0 || static_cast<std::size_t>(written) >= sizeof(buffer)) return {};
    return std::string(buffer, static_cast<std::size_t>(written));
}

}  // namespace

bool valid_offline_career(std::uint32_t career) {
    return career >= kFirstCareer && career <= kLastCareer;
}

std::optional<InitialPoint> initial_manager_point(float win_width, float win_height) {
    if (!std::isfinite(win_width) || !std::isfinite(win_height) ||
            win_width <= 0.0f || win_height <= 0.0f) {
        return std::nullopt;
    }

    InitialPoint point;
    point.x = win_width * 0.5f;
    point.y = win_height * 0.7f;
    if (!std::isfinite(point.x) || !std::isfinite(point.y)) return std::nullopt;
    return point;
}

std::optional<ResourceNames> resource_names(
        std::uint32_t career,
        std::uint32_t player_clothes_id) {
    if (!valid_offline_career(career)) return std::nullopt;

    ResourceNames result;
    result.actions_plist = format_name("p%02u_actions.plist", career);
    if (result.actions_plist.empty()) return std::nullopt;

    for (std::size_t index = 0; index < result.default_sprite_plists.size(); ++index) {
        result.default_sprite_plists[index] = format_name(
            "p%02u_%02u_%02u_default.plist",
            career,
            player_clothes_id,
            static_cast<std::uint32_t>(index + 1u));
        if (result.default_sprite_plists[index].empty()) return std::nullopt;
    }
    return result;
}

std::optional<std::string> action_entry_key(std::uint32_t state_index) {
    if (state_index == std::numeric_limits<std::uint32_t>::max()) return std::nullopt;
    const auto key = format_name("action%02u", state_index + 1u);
    if (key.empty()) return std::nullopt;
    return key;
}

std::string player_name() {
    const auto selected_role = role_selection_state::snapshot();
    if (selected_role.enter_dispatch_committed &&
            !selected_role.dispatched_character_name.empty()) {
        return selected_role.dispatched_character_name;
    }

    const auto created_role = character_name_state::snapshot();
    if (!created_role.role_name.empty()) return created_role.role_name;

    return "SELF";
}

}  // namespace nevergone::game_scene_player_bootstrap
