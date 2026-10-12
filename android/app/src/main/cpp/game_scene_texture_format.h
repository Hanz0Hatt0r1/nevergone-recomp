#pragma once

#include <cstdint>

namespace nevergone::game_scene_texture_format {

enum class PixelFormat : std::uint8_t {
    kRgba8888 = 0,
    kRgba4444 = 1,
};

}  // namespace nevergone::game_scene_texture_format
