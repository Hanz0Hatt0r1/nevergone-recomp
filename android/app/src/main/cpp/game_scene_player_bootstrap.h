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
// creinitWithMEName/initWithMEName for the primary offline player. initWithMEName
// retains 1 at MEPlayer +0x194 and 0 at +0x198, and initializes +0x278 to 0x1b.
constexpr std::uint32_t kInitialResourceSelector = 1u;
constexpr std::uint32_t kInitialComparedResourceField = 0u;
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
// names using the retained MEPlayer +0x194 selector. loadPlayerData() can later
// repopulate that selector from player data; this generic form remains explicit.
std::optional<ResourceNames> resource_names(
        std::uint32_t career,
        std::uint32_t color_file_id);

// First-construction offline resources are not guessed: the manager supplies
// selector 1 to initWithMEName, which immediately uses it for setColorFile().
std::optional<ResourceNames> initial_resource_names(std::uint32_t career);

constexpr const char* player_name() {
    return "SELF";
}

}  // namespace nevergone::game_scene_player_bootstrap
