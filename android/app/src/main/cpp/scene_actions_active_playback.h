#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>

#include "scene_action_pair_document_store.h"
#include "scene_actions_play_state.h"
#include "scene_actions_wa_update.h"
#include "trap_actions_document.h"

namespace nevergone::scene_actions_active_playback {

enum class TickStatus {
    kRejected = 0,
    kNoAdvance,
    kFrameChanged,
    kAwaitingTerminalResolution,
};

struct SlotState {
    bool active = false;
    bool awaiting_terminal_resolution = false;
    int variant = 0;
    scene_action_pair_document_store::Slot slot = scene_action_pair_document_store::Slot::kBorn;
    float parent_x = 0.0f;
    float parent_y = 0.0f;
    int child_z_order = 0;
    scene_actions_play_state::State playback{};
};

struct TickResult {
    TickStatus status = TickStatus::kRejected;
    std::size_t frame_index = 0;
    bool frame_request_valid = false;
    std::size_t frames_advanced = 0;
};

class Runtime {
public:
    bool start(
            scene_action_pair_document_store::Slot slot,
            int variant,
            const trap_actions_document::Document& document,
            float parent_x,
            float parent_y,
            int child_z_order) {
        if (variant < 1 || variant > 4) return false;
        const std::uint8_t mode = slot == scene_action_pair_document_store::Slot::kBorn ? 0u : 1u;
        const auto state = scene_actions_play_state::build(document, mode);
        if (!state.has_value()) return false;

        SlotState next;
        next.active = true;
        next.variant = variant;
        next.slot = slot;
        next.parent_x = parent_x;
        next.parent_y = parent_y;
        next.child_z_order = child_z_order;
        next.playback = *state;
        slots_[index(slot)] = next;
        return true;
    }

    TickResult tick(
            scene_action_pair_document_store::Slot slot,
            const trap_actions_document::Document& document,
            float dt_seconds) {
        TickResult out;
        SlotState& state = slots_[index(slot)];
        if (!state.active || state.awaiting_terminal_resolution) {
            out.status = state.awaiting_terminal_resolution
                    ? TickStatus::kAwaitingTerminalResolution
                    : TickStatus::kRejected;
            return out;
        }

        const auto result = scene_actions_wa_update::advance(document, dt_seconds, &state.playback);
        out.frames_advanced = result.frames_advanced;
        if (result.status == scene_actions_wa_update::Status::kRejected) return out;

        if (result.terminal_boundary_reached) {
            state.awaiting_terminal_resolution = true;
            out.status = TickStatus::kAwaitingTerminalResolution;
            return out;
        }

        out.frame_index = state.playback.frame_index_250;
        out.frame_request_valid = out.frame_index < document.core.primary.records.size();
        if (!out.frame_request_valid) {
            out.status = TickStatus::kRejected;
            return out;
        }

        out.status = result.update_data_requested
                ? TickStatus::kFrameChanged
                : TickStatus::kNoAdvance;
        return out;
    }

    std::optional<SlotState> snapshot(scene_action_pair_document_store::Slot slot) const {
        const SlotState& state = slots_[index(slot)];
        if (!state.active) return std::nullopt;
        return state;
    }

    void stop(scene_action_pair_document_store::Slot slot) {
        slots_[index(slot)] = SlotState{};
    }

    void reset() {
        slots_ = {};
    }

private:
    static std::size_t index(scene_action_pair_document_store::Slot slot) {
        return slot == scene_action_pair_document_store::Slot::kBorn ? 0u : 1u;
    }

    std::array<SlotState, 2> slots_{};
};

}  // namespace nevergone::scene_actions_active_playback
