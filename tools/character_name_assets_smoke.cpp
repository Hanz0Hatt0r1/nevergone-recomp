#include <array>
#include <cassert>
#include <cstdint>
#include <iostream>

#include "character_name_assets.h"

int main() {
    using namespace nevergone::character_name_assets;

    clear();
    const std::uint64_t start_generation = generation();
    assert(!ready());
    assert(!snapshot().ready);

    const std::array<std::uint32_t, 4> pixels{{
        0xff102030u, 0xff405060u, 0xff708090u, 0xffa0b0c0u
    }};

    assert(!upload_image(-1, 2, 2, pixels.data(), pixels.size()));
    assert(!upload_image(kImageCount, 2, 2, pixels.data(), pixels.size()));
    assert(!upload_image(0, 2, 2, pixels.data(), 3));
    assert(!upload_label(0, 0, 2, pixels.data(), pixels.size()));
    assert(generation() == start_generation);

    for (int slot = 0; slot < kImageCount; ++slot) {
        assert(upload_image(slot, 2, 2, pixels.data(), pixels.size()));
    }
    assert(!ready());
    for (int slot = 0; slot < kLabelCount; ++slot) {
        assert(upload_label(slot, 2, 2, pixels.data(), pixels.size()));
    }
    assert(ready());

    const Snapshot staged = snapshot();
    assert(staged.ready);
    assert(staged.generation == start_generation + kImageCount + kLabelCount);
    assert(staged.images[0].width == 2);
    assert(staged.images[0].height == 2);
    assert(staged.images[0].pixels ==
           std::vector<std::uint32_t>(pixels.begin(), pixels.end()));
    assert(staged.labels[2].pixels[3] == 0xffa0b0c0u);

    // Snapshot owns copies: clearing the live store does not mutate the staged
    // renderer generation already copied on the GL thread.
    clear();
    assert(!ready());
    assert(staged.ready);
    assert(staged.images[0].pixels[0] == 0xff102030u);
    assert(generation() == staged.generation + 1);

    std::cout << "character name assets smoke: ok\n";
    return 0;
}
