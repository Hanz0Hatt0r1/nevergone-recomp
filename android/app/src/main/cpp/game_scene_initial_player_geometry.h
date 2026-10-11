#pragma once

#include <cstddef>

#include "game_scene_direct_geometry.h"
#include "game_scene_initial_player_sprite_request.h"
#include "game_scene_render_queue.h"

namespace nevergone::game_scene_initial_player_geometry {

// GameScene::initGameScene() attaches BattleManager at GameScene Z=7 after the
// already-created scene-layer children. The flattened renderer therefore draws
// the player branch after drawable layer-7 sprites and before layer 8.
constexpr std::size_t kBattleManagerGameSceneZ = 7u;

bool build_quad(
        const game_scene_initial_player_sprite_request::Request& request,
        int texture_width,
        int texture_height,
        int surface_width,
        int surface_height,
        game_scene_direct_geometry::Quad* out);

// Returns the number of static drawable texture commands that must be submitted
// before the BattleManager/player branch. The existing static queue is already
// in recovered layer/object traversal order.
std::size_t static_draw_count_before_player(
        const game_scene_render_queue::Queue& queue);

}  // namespace nevergone::game_scene_initial_player_geometry
