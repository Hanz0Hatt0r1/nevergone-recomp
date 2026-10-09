#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

namespace nevergone::choose_hero_role_focus_asset {

struct Asset {
    int width = 0;
    int height = 0;
    std::vector<std::uint32_t> pixels;
};

void clear();
bool upload(
    int width,
    int height,
    const std::uint32_t* pixels,
    std::size_t count);
bool ready();
std::uint64_t generation();
bool copy(Asset* output);

}  // namespace nevergone::choose_hero_role_focus_asset
