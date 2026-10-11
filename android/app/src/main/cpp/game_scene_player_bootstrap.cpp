#include "game_scene_player_bootstrap.h"

#include <cmath>
#include <cstdio>

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
        std::uint32_t color_file_id) {
    if (!valid_offline_career(career)) return std::nullopt;

    ResourceNames result;
    result.actions_plist = format_name("p%02u_actions.plist", career);
    if (result.actions_plist.empty()) return std::nullopt;

    for (std::size_t index = 0; index < result.default_sprite_plists.size(); ++index) {
        result.default_sprite_plists[index] = format_name(
            "p%02u_%02u_%02u_default.plist",
            career,
            color_file_id,
            static_cast<std::uint32_t>(index + 1u));
        if (result.default_sprite_plists[index].empty()) return std::nullopt;
    }
    return result;
}

std::optional<ResourceNames> initial_resource_names(std::uint32_t career) {
    return resource_names(career, kInitialResourceSelector);
}

}  // namespace nevergone::game_scene_player_bootstrap
