#pragma once

#include <cstdint>

namespace nevergone::game_scene_rgba4444 {

// The shipped CCTexture2D format-6 conversion packs the high nibble of each
// already-premultiplied RGBA8888 component as RRRR GGGG BBBB AAAA.
constexpr std::uint16_t pack(
        std::uint8_t red,
        std::uint8_t green,
        std::uint8_t blue,
        std::uint8_t alpha) {
    return static_cast<std::uint16_t>(
            (static_cast<std::uint16_t>(red & 0xf0u) << 8u) |
            (static_cast<std::uint16_t>(green & 0xf0u) << 4u) |
            static_cast<std::uint16_t>(blue & 0xf0u) |
            static_cast<std::uint16_t>(alpha >> 4u));
}

}  // namespace nevergone::game_scene_rgba4444
