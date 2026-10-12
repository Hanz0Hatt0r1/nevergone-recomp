#pragma once

#include <cstddef>
#include <optional>

#include "scene_action_initial_frame_pixel_store.h"
#include "scene_action_pair_document_store.h"
#include "scene_actions_active_playback.h"
#include "scene_actions_base_sprite_update.h"
#include "scene_actions_render_frame_plan.h"

namespace nevergone::scene_actions_render_runtime {

enum class PrepareStatus {
    kReady = 0,
    kInactive,
    kAwaitingTerminalResolution,
    kDocumentUnavailable,
    kPixelsUnavailable,
    kStalePixels,
    kInvalidFrame,
};

struct PreparedFrame {
    scene_actions_render_frame_plan::Plan plan{};
    scene_action_initial_frame_pixel_store::Asset asset{};
    float parent_x = 0.0f;
    float parent_y = 0.0f;
    int child_z_order = 0;
};

struct PrepareResult {
    PrepareStatus status = PrepareStatus::kInactive;
    std::optional<PreparedFrame> frame;
};

// Convert one active playback slot into an exact renderer submission. System
// transform inputs remain explicit because native initialization/mutation of
// SceneActionsSystem +0x240/+0x244/+0x248/+0x15e is a separate evidence boundary.
inline PrepareResult prepare(
        const scene_actions_active_playback::SlotState& state,
        const trap_actions_document::Document* document,
        const scene_action_initial_frame_pixel_store::Asset* asset,
        const scene_actions_base_sprite_update::SystemInputs& system) {
    if (!state.active) return {PrepareStatus::kInactive, std::nullopt};
    if (state.awaiting_terminal_resolution) {
        return {PrepareStatus::kAwaitingTerminalResolution, std::nullopt};
    }
    if (document == nullptr) return {PrepareStatus::kDocumentUnavailable, std::nullopt};
    if (asset == nullptr) return {PrepareStatus::kPixelsUnavailable, std::nullopt};

    const std::size_t frame_index = state.playback.frame_index_250;
    if (asset->variant != state.variant || asset->slot != state.slot ||
            asset->frame_index != frame_index) {
        return {PrepareStatus::kStalePixels, std::nullopt};
    }

    const auto plan = scene_actions_render_frame_plan::build(
            *document,
            *asset,
            state.variant,
            state.slot,
            frame_index,
            system);
    if (!plan.has_value()) return {PrepareStatus::kInvalidFrame, std::nullopt};

    PreparedFrame prepared;
    prepared.plan = *plan;
    prepared.asset = *asset;
    prepared.parent_x = state.parent_x;
    prepared.parent_y = state.parent_y;
    prepared.child_z_order = state.child_z_order;
    return {PrepareStatus::kReady, std::move(prepared)};
}

inline PrepareResult prepare_from_store(
        const scene_actions_active_playback::SlotState& state,
        const scene_actions_base_sprite_update::SystemInputs& system) {
    const auto document = scene_action_pair_document_store::current_document(state.slot);
    if (!document.has_value()) return {PrepareStatus::kDocumentUnavailable, std::nullopt};
    const auto asset = scene_action_initial_frame_pixel_store::current(state.slot);
    if (!asset.has_value()) return {PrepareStatus::kPixelsUnavailable, std::nullopt};
    return prepare(state, &*document, &*asset, system);
}

}  // namespace nevergone::scene_actions_render_runtime
