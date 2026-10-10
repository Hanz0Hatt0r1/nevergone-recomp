#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

namespace nevergone::game_scene_direct_asset_store {

constexpr std::size_t kMaxAssetCount = 4096u;
constexpr std::size_t kMaxPixelsPerAsset = 16u * 1024u * 1024u;
constexpr std::size_t kMaxPendingPixelCount = 32u * 1024u * 1024u;

struct Asset {
    std::size_t request_index = 0;
    std::size_t sprite_command_index = 0;
    int width = 0;
    int height = 0;
    std::vector<std::uint32_t> argb_pixels;
};

struct Snapshot {
    std::uint64_t active_revision = 0;
    std::size_t active_asset_count = 0;
    std::uint64_t pending_revision = 0;
    std::size_t pending_expected_count = 0;
    std::size_t pending_uploaded_count = 0;
    std::size_t pending_pixel_count = 0;
};

void clear();
bool begin(std::uint64_t revision, std::size_t expected_count);
bool upload(
        std::uint64_t revision,
        std::size_t request_index,
        std::size_t sprite_command_index,
        int width,
        int height,
        const std::uint32_t* argb_pixels,
        std::size_t pixel_count);
bool finish(std::uint64_t revision);
void cancel(std::uint64_t revision);
Snapshot snapshot();
bool copy_active_asset(std::size_t request_index, Asset* out);

}  // namespace nevergone::game_scene_direct_asset_store
