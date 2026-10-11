#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

#include "game_levels_scene_instance.h"
#include "game_scene_player_bootstrap.h"

namespace nevergone::game_scene_player_entry_markers {

// One type-6 layer object whose serialized tail value matches the currently
// retained GameLevels event-port type.
struct Marker {
    std::size_t source_layer_index = 0;
    std::size_t source_object_index = 0;
    std::uint32_t event_port_type = 0;
    float source_x = 0.0f;
    float source_y = 0.0f;
};

// This is the exact point passed by GameScene::loadingTex() to
// MEPlayer::setPlayerPos(). MEPlayer stores it at +0x248 and getPlayerPos()
// later adds the construction/bootstrap point retained at +0x240.
struct OfflinePosition {
    float x = 0.0f;
    float y = 0.0f;
};

struct ResolvedOfflinePlayerPosition {
    Marker marker;
    game_scene_player_bootstrap::InitialPoint bootstrap_position;
    OfflinePosition scene_offset;
    game_scene_player_bootstrap::InitialPoint visible_position;
};

// Mirrors the proven selection boundary in GameScene::loadingTex(): visit the
// first 11 ordered layers, inspect objects in source order, and retain type 6
// records whose +0x58 tail value equals GameLevels +0x40 (stored event-port
// type). A malformed matching record fails closed. Multiple matches are kept in
// traversal order because the original executes the position path for each one.
std::optional<std::vector<Marker>> collect(
        const game_levels_scene_instance::SceneInstance& scene,
        std::uint32_t stored_event_port_type);

// Mirrors the recovered offline loadingTex coordinate conversion immediately
// before GameScene +0x4c4 and MEPlayer::setPlayerPos():
//   x' = source_x - win_width * 0.5
//   y' = source_y + 40.0 - win_height * 0.5
// Window dimensions come from CCDirector::getWinSize(). Non-finite values or
// non-positive window dimensions fail closed.
std::optional<OfflinePosition> transform_offline(
        const Marker& marker,
        float win_width,
        float win_height);

// Resolves the visible first-scene player point through the exact two-part
// MEPlayer position model:
//   getPlayerPos() = bootstrap(+0x240) + scene_offset(+0x248)
// loadingTex calls setPlayerPos for every matching type-6 marker, so the final
// state after traversal is determined by the last matching marker.
std::optional<ResolvedOfflinePlayerPosition> resolve_offline_player_position(
        const game_levels_scene_instance::SceneInstance& scene,
        std::uint32_t stored_event_port_type,
        float win_width,
        float win_height);

}  // namespace nevergone::game_scene_player_entry_markers
