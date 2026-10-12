#pragma once

#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>

#include "scene_action_initial_frame_pixel_store.h"
#include "scene_actions_primary_frame_plan.h"
#include "trap_actions_document.h"

namespace nevergone::scene_actions_render_frame_plan {

struct Plan {
    scene_actions_primary_frame_plan::Plan geometry;
    int variant = 0;
    scene_action_pair_document_store::Slot slot =
            scene_action_pair_document_store::Slot::kBorn;
    std::uint8_t mode_24e = 0;
    std::size_t frame_index = 0;
    int width = 0;
    int height = 0;
    std::size_t pixel_count = 0;
    std::uint64_t pixel_generation = 0;
};

// Compose one renderer-facing SceneActions frame only when the retained ARGB
// pixels identify the exact same primary frame as the geometry descriptor.
// This prevents a waUpdate cursor advance from drawing stale pixels while a new
// Android atlas decode is still in flight.
inline std::optional<Plan> build(
        const trap_actions_document::Document& document,
        const scene_action_initial_frame_pixel_store::Asset& asset,
        int expected_variant,
        scene_action_pair_document_store::Slot expected_slot,
        std::size_t expected_frame_index,
        const scene_actions_base_sprite_update::SystemInputs& system) {
    if (expected_variant < 1 || expected_variant > 4 ||
            asset.variant != expected_variant || asset.slot != expected_slot ||
            asset.frame_index != expected_frame_index ||
            asset.mode_24e != (expected_slot == scene_action_pair_document_store::Slot::kBorn ? 0u : 1u) ||
            asset.width <= 0 || asset.height <= 0 || asset.argb_pixels.empty()) {
        return std::nullopt;
    }

    const std::size_t width = static_cast<std::size_t>(asset.width);
    const std::size_t height = static_cast<std::size_t>(asset.height);
    if (width > std::numeric_limits<std::size_t>::max() / height) return std::nullopt;
    const std::size_t expected_pixels = width * height;
    if (expected_pixels == 0u || expected_pixels != asset.argb_pixels.size()) return std::nullopt;

    const auto geometry = scene_actions_primary_frame_plan::build(
            document,
            expected_frame_index,
            system);
    if (!geometry.has_value()) return std::nullopt;

    Plan plan;
    plan.geometry = *geometry;
    plan.variant = asset.variant;
    plan.slot = asset.slot;
    plan.mode_24e = asset.mode_24e;
    plan.frame_index = asset.frame_index;
    plan.width = asset.width;
    plan.height = asset.height;
    plan.pixel_count = asset.argb_pixels.size();
    plan.pixel_generation = asset.generation;
    return plan;
}

}  // namespace nevergone::scene_actions_render_frame_plan
