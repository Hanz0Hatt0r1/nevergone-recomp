#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace nevergone::character_name_assets {

enum class ImageSlot : int {
    kBackground = 0,
    kNamePlate = 1,
    kRandomButton = 2,
    kFixedButtonNormal = 3,
    kFixedButtonPressed = 4,
    kFixedButtonDisabled = 5,
};

enum class LabelSlot : int {
    kTitle = 0,
    kConfirm = 1,
    kCancel = 2,
};

constexpr int kImageCount = 6;
constexpr int kLabelCount = 3;

struct Asset {
    int width = 0;
    int height = 0;
    std::vector<std::uint32_t> pixels;
};

struct Snapshot {
    std::uint64_t generation = 0;
    bool ready = false;
    std::array<Asset, kImageCount> images;
    std::array<Asset, kLabelCount> labels;
};

void clear();
bool upload_image(
    int slot,
    int width,
    int height,
    const std::uint32_t* pixels,
    std::size_t count);
bool upload_label(
    int slot,
    int width,
    int height,
    const std::uint32_t* pixels,
    std::size_t count);

bool ready();
std::uint64_t generation();
Snapshot snapshot();

}  // namespace nevergone::character_name_assets
