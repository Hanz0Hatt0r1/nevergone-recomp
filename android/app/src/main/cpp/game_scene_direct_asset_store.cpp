#include "game_scene_direct_asset_store.h"

#include <mutex>
#include <optional>
#include <utility>
#include <vector>

namespace nevergone::game_scene_direct_asset_store {
namespace {

std::mutex g_mutex;
std::uint64_t g_active_revision = 0;
std::vector<Asset> g_active_assets;
std::uint64_t g_pending_revision = 0;
std::vector<std::optional<Asset>> g_pending_assets;
std::size_t g_pending_uploaded_count = 0;
std::size_t g_pending_pixel_count = 0;

void clear_pending_locked() {
    g_pending_revision = 0;
    g_pending_assets.clear();
    g_pending_uploaded_count = 0;
    g_pending_pixel_count = 0;
}

bool dimensions_match(int width, int height, std::size_t pixel_count) {
    if (width <= 0 || height <= 0) return false;
    const std::size_t w = static_cast<std::size_t>(width);
    const std::size_t h = static_cast<std::size_t>(height);
    if (w > kMaxPixelsPerAsset / h) return false;
    const std::size_t expected = w * h;
    return expected == pixel_count && expected <= kMaxPixelsPerAsset;
}

bool supported_format(game_scene_texture_format::PixelFormat pixel_format) {
    using game_scene_texture_format::PixelFormat;
    return pixel_format == PixelFormat::kRgba8888 ||
            pixel_format == PixelFormat::kRgba4444;
}

}  // namespace

void clear() {
    std::lock_guard<std::mutex> lock(g_mutex);
    g_active_revision = 0;
    g_active_assets.clear();
    clear_pending_locked();
}

bool begin(std::uint64_t revision, std::size_t expected_count) {
    if (revision == 0 || expected_count > kMaxAssetCount) return false;
    std::lock_guard<std::mutex> lock(g_mutex);
    g_pending_revision = revision;
    g_pending_assets.assign(expected_count, std::nullopt);
    g_pending_uploaded_count = 0;
    g_pending_pixel_count = 0;
    return true;
}

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
        std::size_t pixel_count) {
    if (argb_pixels == nullptr ||
        !supported_format(pixel_format) ||
        !dimensions_match(width, height, pixel_count)) {
        return false;
    }

    std::lock_guard<std::mutex> lock(g_mutex);
    if (revision == 0 || revision != g_pending_revision ||
        request_index >= g_pending_assets.size()) {
        return false;
    }

    const auto& previous = g_pending_assets[request_index];
    const std::size_t previous_pixels = previous.has_value()
            ? previous->argb_pixels.size()
            : 0u;
    const std::size_t base_pixels = g_pending_pixel_count - previous_pixels;
    if (pixel_count > kMaxPendingPixelCount - base_pixels) return false;

    Asset asset;
    asset.request_index = request_index;
    asset.sprite_command_index = sprite_command_index;
    asset.pixel_format = pixel_format;
    asset.has_frame_offset = has_frame_offset;
    asset.frame_offset_x = has_frame_offset ? frame_offset_x : 0;
    asset.frame_offset_y = has_frame_offset ? frame_offset_y : 0;
    asset.width = width;
    asset.height = height;
    asset.argb_pixels.assign(argb_pixels, argb_pixels + pixel_count);

    if (!previous.has_value()) ++g_pending_uploaded_count;
    g_pending_assets[request_index] = std::move(asset);
    g_pending_pixel_count = base_pixels + pixel_count;
    return true;
}

bool finish(std::uint64_t revision) {
    std::lock_guard<std::mutex> lock(g_mutex);
    if (revision == 0 || revision != g_pending_revision ||
        g_pending_uploaded_count != g_pending_assets.size()) {
        return false;
    }

    std::vector<Asset> completed;
    completed.reserve(g_pending_assets.size());
    for (auto& slot : g_pending_assets) {
        if (!slot.has_value()) return false;
        completed.push_back(std::move(*slot));
    }

    g_active_revision = revision;
    g_active_assets = std::move(completed);
    clear_pending_locked();
    return true;
}

void cancel(std::uint64_t revision) {
    std::lock_guard<std::mutex> lock(g_mutex);
    if (revision != 0 && revision == g_pending_revision) clear_pending_locked();
}

Snapshot snapshot() {
    std::lock_guard<std::mutex> lock(g_mutex);
    Snapshot state;
    state.active_revision = g_active_revision;
    state.active_asset_count = g_active_assets.size();
    state.pending_revision = g_pending_revision;
    state.pending_expected_count = g_pending_assets.size();
    state.pending_uploaded_count = g_pending_uploaded_count;
    state.pending_pixel_count = g_pending_pixel_count;
    return state;
}

bool copy_active_asset(std::size_t request_index, Asset* out) {
    if (out == nullptr) return false;
    std::lock_guard<std::mutex> lock(g_mutex);
    if (g_active_revision == 0 || request_index >= g_active_assets.size()) {
        *out = {};
        return false;
    }
    const Asset& asset = g_active_assets[request_index];
    if (asset.request_index != request_index) {
        *out = {};
        return false;
    }
    *out = asset;
    return true;
}

}  // namespace nevergone::game_scene_direct_asset_store
