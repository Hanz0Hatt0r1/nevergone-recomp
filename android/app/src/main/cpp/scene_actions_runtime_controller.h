#pragma once

#include <array>
#include <cstdint>
#include <optional>

#include "scene_action_pair_document_store.h"
#include "scene_action_trigger_plan.h"
#include "scene_action_trigger_runtime.h"
#include "scene_actions_active_playback.h"
#include "scene_actions_play_state.h"
#include "trap_actions_document.h"

namespace nevergone::scene_actions_runtime_controller {

enum class StartStatus {
    kStarted = 0,
    kAlreadyConsumed,
    kPairUnavailable,
    kInvalidPlan,
};

struct StartResult {
    StartStatus status = StartStatus::kInvalidPlan;
    int variant = 0;
};

class Controller {
public:
    StartResult start_from_store(
            const scene_action_trigger_plan::Plan& plan,
            int variant) {
        const auto pair = scene_action_pair_document_store::snapshot();
        if (!pair.pair_ready || pair.variant != variant || variant < 1 || variant > 4) {
            return {StartStatus::kPairUnavailable, variant};
        }
        const auto born = scene_action_pair_document_store::current_document(
                scene_action_pair_document_store::Slot::kBorn);
        const auto idle = scene_action_pair_document_store::current_document(
                scene_action_pair_document_store::Slot::kIdle);
        if (!born.has_value() || !idle.has_value()) {
            return {StartStatus::kPairUnavailable, variant};
        }
        return start_with_documents(plan, variant, *born, *idle);
    }

    StartResult start_with_documents(
            const scene_action_trigger_plan::Plan& plan,
            int variant,
            const trap_actions_document::Document& born_document,
            const trap_actions_document::Document& idle_document) {
        if (variant < 1 || variant > 4) {
            return {StartStatus::kPairUnavailable, variant};
        }

        // Validate both play states before consuming the one-shot trigger gate.
        if (!scene_actions_play_state::build(born_document, 0u).has_value() ||
                !scene_actions_play_state::build(idle_document, 1u).has_value()) {
            return {StartStatus::kPairUnavailable, variant};
        }

        std::array<scene_action_trigger_runtime::Command,
                   scene_action_trigger_plan::kSlotCount> commands{};
        const auto triggered = trigger_runtime_.trigger(plan, &commands);
        if (triggered == scene_action_trigger_runtime::TriggerResult::kAlreadyConsumed) {
            return {StartStatus::kAlreadyConsumed, variant};
        }
        if (triggered != scene_action_trigger_runtime::TriggerResult::kStarted) {
            return {StartStatus::kInvalidPlan, variant};
        }

        const auto& born = commands[0];
        const auto& idle = commands[1];
        if (!playback_.start(
                    scene_action_pair_document_store::Slot::kBorn,
                    variant,
                    born_document,
                    born.local_x,
                    born.local_y,
                    born.child_z_order) ||
                !playback_.start(
                    scene_action_pair_document_store::Slot::kIdle,
                    variant,
                    idle_document,
                    idle.local_x,
                    idle.local_y,
                    idle.child_z_order)) {
            // This should be unreachable after the pre-validation above, but
            // leave the runtime fail-closed if the contracts ever diverge.
            playback_.reset();
            return {StartStatus::kInvalidPlan, variant};
        }

        variant_ = variant;
        started_ = true;
        return {StartStatus::kStarted, variant};
    }

    scene_actions_active_playback::TickResult tick(
            scene_action_pair_document_store::Slot slot,
            float dt_seconds) {
        if (!started_) return {};
        const auto document = scene_action_pair_document_store::current_document(slot);
        if (!document.has_value()) return {};
        return playback_.tick(slot, *document, dt_seconds);
    }

    scene_actions_active_playback::TickResult tick_with_document(
            scene_action_pair_document_store::Slot slot,
            const trap_actions_document::Document& document,
            float dt_seconds) {
        if (!started_) return {};
        return playback_.tick(slot, document, dt_seconds);
    }

    std::optional<scene_actions_active_playback::SlotState> snapshot(
            scene_action_pair_document_store::Slot slot) const {
        return playback_.snapshot(slot);
    }

    bool started() const { return started_; }
    int variant() const { return variant_; }

    void reset() {
        trigger_runtime_.reset();
        playback_.reset();
        started_ = false;
        variant_ = 0;
    }

private:
    scene_action_trigger_runtime::Runtime trigger_runtime_{};
    scene_actions_active_playback::Runtime playback_{};
    bool started_ = false;
    int variant_ = 0;
};

}  // namespace nevergone::scene_actions_runtime_controller
