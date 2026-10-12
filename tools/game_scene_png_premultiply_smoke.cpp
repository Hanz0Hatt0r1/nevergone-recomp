#include <cassert>
#include <cstdint>

#include "game_scene_png_premultiply.h"

int main() {
    using nevergone::game_scene_png_premultiply::channel;

    static_assert(channel(0u, 0u) == 0u);
    static_assert(channel(255u, 255u) == 255u);
    static_assert(channel(127u, 255u) == 127u);

    // Exact shipped integer transform: value * (alpha + 1) >> 8.
    assert(channel(255u, 0u) == 0u);
    assert(channel(255u, 127u) == 127u);
    assert(channel(200u, 128u) == 100u);
    assert(channel(17u, 64u) == 4u);

    // Alpha 255 must be an identity mapping for every 8-bit channel.
    for (std::uint32_t value = 0; value <= 255u; ++value) {
        assert(channel(static_cast<std::uint8_t>(value), 255u) == value);
    }

    return 0;
}
