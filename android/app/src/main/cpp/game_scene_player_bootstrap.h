#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <string>

namespace nevergone::game_scene_player_bootstrap {

constexpr std::uint32_t kFirstCareer = 1u;
constexpr std::uint32_t kLastCareer = 5u;
constexpr std::size_t kDefaultSpritePlistCount = 9u;

// MEPlayerManager::initMEPlayerManager passes these exact bootstrap values to
// creinitWithMEName/initWithMEName for the primary offline player. The recovered
// MEPlayerData vtable identifies MEPlayer +0x194 as the player-clothes id and
// +0x198 as the player-color id after loadPlayerData() refreshes them. The
// construction call seeds those fields with 1 and 0 respectively and initializes
// +0x278 to action state 0x1b.
constexpr std::uint32_t kConstructionPlayerClothesId = 1u;
constexpr std::uint32_t kConstructionPlayerColorId = 0u;
constexpr std::uint32_t kInitialActionState = 0x1bu;

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
// names using the player-clothes id retained at MEPlayer +0x194 at that point.
// Callers must supply the recovered value explicitly; this layer never assumes
// the construction-time clothes id survives player-data loading.
std::optional<ResourceNames> resource_names(
        std::uint32_t career,
        std::uint32_t player_clothes_id);

constexpr const char* player_name() {
    return "SELF";
}

}  // namespace nevergone::game_scene_player_bootstrap
