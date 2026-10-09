#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

namespace nevergone::choose_hero_profile_assets {

constexpr int kLabelCount = 3;

enum LabelKind : int {
    kName = 0,
    kLevel = 1,
    kPlayTime = 2,
};

struct Asset {
    int width = 0;
    int height = 0;
    std::vector<std::uint32_t> pixels;
};

void clear();
bool upload_background(
    int width,
    int height,
    const std::uint32_t* pixels,
    std::size_t count);
bool upload_label(
    std::uint32_t slot_id,
    int kind,
    int width,
    int height,
    const std::uint32_t* pixels,
    std::size_t count);

bool background_ready();
bool profile_ready(std::uint32_t slot_id);
std::uint64_t generation();
bool copy_background(Asset* output);
bool copy_label(std::uint32_t slot_id, int kind, Asset* output);

}  // namespace nevergone::choose_hero_profile_assets
