#pragma once

#include <cstddef>

#include "game_scene_direct_texture_cache.h"

namespace nevergone::game_scene_direct_texture_gl {

void on_surface_created();
bool sync();
void clear();
game_scene_direct_texture_cache::Snapshot snapshot();
bool texture_for_sprite_command(
        std::size_t sprite_command_index,
        game_scene_direct_texture_cache::Texture* out);

}  // namespace nevergone::game_scene_direct_texture_gl
