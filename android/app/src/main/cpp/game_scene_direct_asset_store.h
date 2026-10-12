#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

#include "game_scene_texture_format.h"

namespace nevergone::game_scene_direct_asset_store {

constexpr std::size_t kMaxAssetCount = 4096u;
constexpr std::size_t kMaxPixelsPerAsset = 16u * 1024u * 1024u;
constexpr std::size_t kMaxPendingPixelCount = 32u * 1024u * 1024u;

struct Asset {
    std::size_t request_index = 0;
    std::size_t sprite_command_index = 0;
    game_scene_texture_format::PixelFormat pixel_format =
            game_scene_texture_format::PixelFormat::kRgba8888;
    bool has_frame_offset = false;
    int frame_offset_x = 0;
    int frame_offset_y = 0;
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
        game_scene_texture_format::PixelFormat pixel_format,
        bool has_frame_offset,
        int frame_offset_x,
        int frame_offset_y,
        int width,
        int height,
        const std::uint32_t* argb_pixels,
        std::size_t pixel_count);

// Existing callers without frame-layout metadata retain the standalone-image
// behavior. Atlas staging uses the explicit overload after rotating a packed
// frame into logical sprite orientation.
inline bool upload(
        std::uint64_t revision,
        std::size_t request_index,
        std::size_t sprite_command_index,
        game_scene_texture_format::PixelFormat pixel_format,
        int width,
        int height,
        const std::uint32_t* argb_pixels,
        std::size_t pixel_count) {
    return upload(
            revision,
            request_index,
            sprite_command_index,
            pixel_format,
            false,
            0,
            0,
            width,
            height,
            argb_pixels,
            pixel_count);
}

inline bool upload(
        std::uint64_t revision,
        std::size_t request_index,
        std::size_t sprite_command_index,
        int width,
        int height,
        const std::uint32_t* argb_pixels,
        std::size_t pixel_count) {
    return upload(
            revision,
            request_index,
            sprite_command_index,
            game_scene_texture_format::PixelFormat::kRgba8888,
            width,
            height,
            argb_pixels,
            pixel_count);
}

bool finish(std::uint64_t revision);
void cancel(std::uint64_t revision);
Snapshot snapshot();
bool copy_active_asset(std::size_t request_index, Asset* out);

}  // namespace nevergone::game_scene_direct_asset_store
