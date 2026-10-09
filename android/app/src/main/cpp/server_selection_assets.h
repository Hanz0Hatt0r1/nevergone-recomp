#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

namespace nevergone::server_selection_assets {

enum AssetIndex : int {
    kBorder1 = 0,
    kBorder2 = 1,
    kAssetCount = 2,
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

bool ready(int index);
bool row_ready();
std::uint64_t generation();
bool copy(int index, Asset* output);

}  // namespace nevergone::server_selection_assets
