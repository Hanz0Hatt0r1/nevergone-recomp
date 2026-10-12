#include "scene_action_initial_frame_pixel_store.h"

#include <limits>
#include <mutex>
#include <utility>

namespace nevergone::scene_action_initial_frame_pixel_store {
namespace {

std::mutex g_mutex;
std::optional<Asset> g_born;
std::optional<Asset> g_idle;
std::uint64_t g_generation = 0;

std::optional<Asset>& slot_asset(scene_action_pair_document_store::Slot slot) {
    return slot == scene_action_pair_document_store::Slot::kBorn ? g_born : g_idle;
}

const std::optional<Asset>& slot_asset_const(scene_action_pair_document_store::Slot slot) {
    return slot == scene_action_pair_document_store::Slot::kBorn ? g_born : g_idle;
}

std::uint8_t expected_mode(scene_action_pair_document_store::Slot slot) {
    return slot == scene_action_pair_document_store::Slot::kBorn ? 0u : 1u;
}

}  // namespace

void reset() {
    std::lock_guard<std::mutex> lock(g_mutex);
    g_born.reset();
    g_idle.reset();
    ++g_generation;
}

bool upload(
        scene_action_pair_document_store::Slot slot,
        int variant,
        std::uint8_t mode_24e,
        int width,
        int height,
        const std::uint32_t* pixels,
        std::size_t pixel_count) {
    if (variant < 1 || variant > 4 || mode_24e != expected_mode(slot) ||
            width <= 0 || height <= 0 || pixels == nullptr) {
        return false;
    }
    const std::size_t w = static_cast<std::size_t>(width);
    const std::size_t h = static_cast<std::size_t>(height);
    if (w > std::numeric_limits<std::size_t>::max() / h) return false;
    const std::size_t expected = w * h;
    if (expected == 0u || expected > kMaxPixelsPerFrame || pixel_count != expected) {
        return false;
    }

    const auto pair = scene_action_pair_document_store::snapshot();
    if (!pair.pair_ready || pair.variant != variant) return false;

    Asset staged;
    staged.variant = variant;
    staged.slot = slot;
    staged.mode_24e = mode_24e;
    staged.width = width;
    staged.height = height;
    staged.argb_pixels.assign(pixels, pixels + expected);

    std::lock_guard<std::mutex> lock(g_mutex);
    staged.generation = ++g_generation;
    slot_asset(slot) = std::move(staged);
    return true;
}

std::optional<Asset> current(scene_action_pair_document_store::Slot slot) {
    std::lock_guard<std::mutex> lock(g_mutex);
    return slot_asset_const(slot);
}

bool ready(scene_action_pair_document_store::Slot slot, int variant) {
    std::lock_guard<std::mutex> lock(g_mutex);
    const auto& asset = slot_asset_const(slot);
    return asset.has_value() && asset->variant == variant &&
            asset->width > 0 && asset->height > 0 && !asset->argb_pixels.empty();
}

}  // namespace nevergone::scene_action_initial_frame_pixel_store
