#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

namespace nevergone::choose_hero_background {

constexpr int kFrameCount = 10;
constexpr int kDesignPhaseFrameCount = 7;
constexpr int kDesignWidth = 1136;
constexpr int kDesignHeight = 640;
constexpr int kStormBackgroundIndex = 5;

struct FrameAsset {
    int width = 0;
    int height = 0;
    int left = 0;
    int top = 0;
    int source_width = 0;
    int source_height = 0;
    std::vector<std::uint32_t> pixels;
};

void clear();
bool upload(
    int index,
    int width,
    int height,
    int left,
    int top,
    int source_width,
    int source_height,
    const std::uint32_t* pixels,
    std::size_t count);
bool ready();
bool route_active();
std::uint64_t generation();
bool copy_frame(int index, FrameAsset* output);

}  // namespace nevergone::choose_hero_background
