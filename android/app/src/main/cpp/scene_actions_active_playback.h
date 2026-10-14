#pragma once

#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <optional>

#include "scene_action_pair_document_store.h"
#include "scene_actions_play_state.h"
#include "scene_actions_terminal_transition.h"
#include "scene_actions_wa_update.h"
#include "trap_actions_document.h"

namespace nevergone::scene_actions_active_playback {

enum class TickStatus {
    kRejected = 0,
    kNoAdvance,
    kFrameChanged,
    kCompleted,
    kLooped,
};

struct SlotState {
    bool active = false;
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
        if (!valid_slot(slot) || variant < 1 || variant > 4 ||
                !std::isfinite(parent_x) || !std::isfinite(parent_y)) return false;
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
        if (!valid_slot(slot)) return out;
        SlotState& state = slots_[index(slot)];
        if (!state.active) return out;

        // Born mode remains retained but done after its terminal branch. The
        // native sprite keeps its most recently applied display frame because
        // that branch does not call updateData().
        if (state.playback.done_1a8) {
            out.status = TickStatus::kCompleted;
            return out;
        }

        const auto result = scene_actions_wa_update::advance(document, dt_seconds, &state.playback);
        out.frames_advanced = result.frames_advanced;
        if (result.status == scene_actions_wa_update::Status::kRejected) return out;

        if (result.terminal_boundary_reached) {
            // scene_actions_wa_update currently exposes a project-owned edge at
            // the end of the parsed primary vector. Do not reinterpret that
            // edge as native count semantics here; only apply the separately
            // recovered state writes once the existing edge has fired.
            const auto terminal = scene_actions_terminal_transition::apply(
                    document.core.primary.records.size(), &state.playback);
            if (!terminal.accepted) return out;

            if (terminal.completed) {
                out.status = TickStatus::kCompleted;
                return out;
            }
            if (terminal.looped_to_frame_zero && terminal.update_data_requested) {
                out.status = TickStatus::kLooped;
                out.frame_index = 0u;
                out.frame_request_valid = !document.core.primary.records.empty();
                return out;
            }
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
        if (!valid_slot(slot)) return std::nullopt;
        const SlotState& state = slots_[index(slot)];
        if (!state.active) return std::nullopt;
        return state;
    }

    void stop(scene_action_pair_document_store::Slot slot) {
        if (!valid_slot(slot)) return;
        slots_[index(slot)] = SlotState{};
    }

    void reset() {
        slots_ = {};
    }

private:
    static bool valid_slot(scene_action_pair_document_store::Slot slot) {
        return slot == scene_action_pair_document_store::Slot::kBorn ||
                slot == scene_action_pair_document_store::Slot::kIdle;
    }

    static std::size_t index(scene_action_pair_document_store::Slot slot) {
        return slot == scene_action_pair_document_store::Slot::kBorn ? 0u : 1u;
    }

    std::array<SlotState, 2> slots_{};
};

}  // namespace nevergone::scene_actions_active_playback
