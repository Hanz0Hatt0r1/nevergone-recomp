#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

namespace nevergone::choose_hero_role_item_assets {

constexpr int kAssetCount = 8;

enum AssetIndex : int {
    kBoardA = 0,
    kBoardB = 1,
    kHero01A = 2,
    kHero01B = 3,
    kHero02A = 4,
    kHero02B = 5,
    kCreateA = 6,
    kCreateB = 7,
};

struct Asset {
    int width = 0;
    int height = 0;
    std::vector<std::uint32_t> pixels;
};

void clear();
bool upload(
    int index,
    int width,
    int height,
    const std::uint32_t* pixels,
    std::size_t count);
bool ready();
std::uint64_t generation();
bool copy(int index, Asset* output);

}  // namespace nevergone::choose_hero_role_item_assets
