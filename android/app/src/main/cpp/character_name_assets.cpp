#include "character_name_assets.h"

#include <mutex>
#include <utility>

namespace nevergone::character_name_assets {
namespace {

std::mutex g_mutex;
std::array<Asset, kImageCount> g_images{};
std::array<Asset, kLabelCount> g_labels{};
std::uint64_t g_generation = 0;

bool valid(const Asset& asset) {
    return asset.width > 0 && asset.height > 0 &&
        asset.pixels.size() ==
            static_cast<std::size_t>(asset.width) * static_cast<std::size_t>(asset.height);
}

bool make_asset(
        int width,
        int height,
        const std::uint32_t* pixels,
        std::size_t count,
        Asset* output) {
    if (output == nullptr || width <= 0 || height <= 0 || pixels == nullptr ||
            count != static_cast<std::size_t>(width) * static_cast<std::size_t>(height) ||
            count > 16777216u) {
        return false;
    }
    Asset result;
    result.width = width;
    result.height = height;
    result.pixels.assign(pixels, pixels + count);
    if (!valid(result)) return false;
    *output = std::move(result);
    return true;
}

bool all_ready_locked() {
    for (const Asset& asset : g_images) {
        if (!valid(asset)) return false;
    }
    for (const Asset& asset : g_labels) {
        if (!valid(asset)) return false;
    }
    return true;
}

}  // namespace

void clear() {
    std::lock_guard<std::mutex> lock(g_mutex);
    for (Asset& asset : g_images) asset = {};
    for (Asset& asset : g_labels) asset = {};
    ++g_generation;
}

bool upload_image(
        int slot,
        int width,
        int height,
        const std::uint32_t* pixels,
        std::size_t count) {
    if (slot < 0 || slot >= kImageCount) return false;
    Asset asset;
    if (!make_asset(width, height, pixels, count, &asset)) return false;
    std::lock_guard<std::mutex> lock(g_mutex);
    g_images[static_cast<std::size_t>(slot)] = std::move(asset);
    ++g_generation;
    return true;
}

bool upload_label(
        int slot,
        int width,
        int height,
        const std::uint32_t* pixels,
        std::size_t count) {
    if (slot < 0 || slot >= kLabelCount) return false;
    Asset asset;
    if (!make_asset(width, height, pixels, count, &asset)) return false;
    std::lock_guard<std::mutex> lock(g_mutex);
    g_labels[static_cast<std::size_t>(slot)] = std::move(asset);
    ++g_generation;
    return true;
}

bool ready() {
    std::lock_guard<std::mutex> lock(g_mutex);
    return all_ready_locked();
}

std::uint64_t generation() {
    std::lock_guard<std::mutex> lock(g_mutex);
    return g_generation;
}

Snapshot snapshot() {
    std::lock_guard<std::mutex> lock(g_mutex);
    Snapshot result;
    result.generation = g_generation;
    result.ready = all_ready_locked();
    result.images = g_images;
    result.labels = g_labels;
    return result;
}

}  // namespace nevergone::character_name_assets
