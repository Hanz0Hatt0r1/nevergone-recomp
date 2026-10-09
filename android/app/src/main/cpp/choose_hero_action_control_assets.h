#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

namespace nevergone::choose_hero_action_control_assets {

enum ButtonAssetIndex : int {
    kButtonNormal = 0,
    kButtonPressed = 1,
    kButtonDisabled = 2,
    kButtonAssetCount = 3,
};

enum LabelIndex : int {
    kPlayLabel = 0,
    kDeleteLabel = 1,
    kLabelCount = 2,
};

struct Asset {
    int width = 0;
    int height = 0;
    std::vector<std::uint32_t> pixels;
};

void clear();
bool upload_button(int index, int width, int height,
                   const std::uint32_t* pixels, std::size_t count);
bool upload_label(int index, int width, int height,
                  const std::uint32_t* pixels, std::size_t count);
bool ready();
std::uint64_t generation();
bool copy_button(int index, Asset* output);
bool copy_label(int index, Asset* output);

}  // namespace nevergone::choose_hero_action_control_assets
