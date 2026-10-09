#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

namespace nevergone::server_selection_assets {

enum AssetIndex : int {
    kBorder1 = 0,
    kBorder2 = 1,
    kButtonNormal = 2,
    kButtonPressed = 3,
    kButtonDisabled = 4,
    kStartLabel = 5,
    kAssetCount = 6,
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
bool confirm_ready();
std::uint64_t generation();
bool dimensions(int index, int* width, int* height);
bool copy(int index, Asset* output);

}  // namespace nevergone::server_selection_assets
