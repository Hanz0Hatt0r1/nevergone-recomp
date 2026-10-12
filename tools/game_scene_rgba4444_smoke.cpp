#include <cassert>
#include <cstdint>

#include "game_scene_png_premultiply.h"
#include "game_scene_rgba4444.h"

int main() {
    using nevergone::game_scene_png_premultiply::channel;
    using nevergone::game_scene_rgba4444::pack;

    static_assert(pack(0xffu, 0xffu, 0xffu, 0xffu) == 0xffffu);
    static_assert(pack(0x12u, 0x34u, 0x56u, 0x78u) == 0x1357u);
    static_assert(pack(0x00u, 0x00u, 0x00u, 0x00u) == 0x0000u);

    // The original path premultiplies at 8-bit precision first, then truncates
    // to the high nibble for CCTexture2D pixel-format value 6.
    const std::uint8_t alpha = 0x80u;
    const std::uint8_t red = channel(0xffu, alpha);
    const std::uint8_t green = channel(0x80u, alpha);
    const std::uint8_t blue = channel(0x40u, alpha);
    assert(red == 0x80u);
    assert(green == 0x40u);
    assert(blue == 0x20u);
    assert(pack(red, green, blue, alpha) == 0x8428u);

    return 0;
}
