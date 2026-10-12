#pragma once

#include <cstdint>

namespace nevergone::game_scene_png_premultiply {

// CCImage::_initWithPngData in the shipped cocos2d-x build uses this exact
// integer operation for each RGB byte of four-channel PNG data.
constexpr std::uint8_t channel(std::uint8_t value, std::uint8_t alpha) {
    return static_cast<std::uint8_t>(
            (static_cast<std::uint32_t>(value) *
             (static_cast<std::uint32_t>(alpha) + 1u)) >> 8u);
}

}  // namespace nevergone::game_scene_png_premultiply
