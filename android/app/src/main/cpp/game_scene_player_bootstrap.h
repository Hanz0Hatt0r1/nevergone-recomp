#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <string>

namespace nevergone::game_scene_player_bootstrap {

constexpr std::uint32_t kFirstCareer = 1u;
constexpr std::uint32_t kLastCareer = 5u;
constexpr std::size_t kDefaultSpritePlistCount = 9u;

struct InitialPoint {
    float x = 0.0f;
    float y = 0.0f;
};

struct ResourceNames {
    std::string actions_plist;
    std::array<std::string, kDefaultSpritePlistCount> default_sprite_plists;
};

// SingleSelectHero::menuConfirm forwards the selected Career value to the first
// MEPlayerManager singleton construction and then to MEPlayer::setPlayerIdx().
bool valid_offline_career(std::uint32_t career);

// MEPlayerManager::initMEPlayerManager constructs the initial MEPlayer point at
// (0.5 * width, 0.7 * height). This is an allocation/bootstrap point; the later
// type-6 GameScene loading path replaces the live scene position.
std::optional<InitialPoint> initial_manager_point(float win_width, float win_height);

// MEPlayer::createActions(player_idx - 1) formats these exact project resource
// names. color_file_id is intentionally an input: setPlayerIdx obtains it from
// DataManager player data before setColorFile()/initPlayerRes(), so this layer
// must not invent a default value.
std::optional<ResourceNames> resource_names(
        std::uint32_t career,
        std::uint32_t color_file_id);

constexpr const char* player_name() {
    return "SELF";
}

}  // namespace nevergone::game_scene_player_bootstrap
