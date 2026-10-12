#pragma once

#include <optional>

#include "game_scene_render_queue.h"
#include "scene_action_pair_document_store.h"
#include "scene_actions_active_playback.h"
#include "scene_actions_current_scene_binding.h"
#include "scene_actions_render_submit.h"
#include "scene_actions_runtime_controller.h"

namespace nevergone::scene_actions_live_runtime {

enum class CurrentSceneStatus {
    kReady = 0,
    kSceneUnavailable,
    kNoPair,
    kAmbiguousPairs,
    kInvalidPair,
};

enum class StartStatus {
    kStarted = 0,
    kAlreadyStarted,
    kSceneUnavailable,
    kNoPair,
    kAmbiguousPairs,
    kInvalidPair,
    kPairUnavailable,
    kAlreadyConsumed,
    kControllerRejected,
};

struct StartResult {
    StartStatus status = StartStatus::kControllerRejected;
    int variant = 0;
    scene_actions_current_scene_binding::Status binding_status =
            scene_actions_current_scene_binding::Status::kNoPair;
    scene_actions_runtime_controller::StartStatus controller_status =
            scene_actions_runtime_controller::StartStatus::kInvalidPlan;
};

// Read-only current-scene preflight. This resolves only the retained project
// render queue and klhuo pair cardinality/shape; it does not inspect/stage a
// variant, consume the trigger gate, mutate playback, or submit GLES state.
CurrentSceneStatus current_scene_status();

// Explicit trigger boundary for the currently retained GameLevels scene. The
// caller supplies the already recovered/staged variant; this API does not draw
// from a PRNG and does not invent when GameSceneObject::triggerSceneAction()
// should fire.
StartResult start_current_scene(int variant);

// Queue-injected form used by deterministic callers/tests. Parsed born/idle
// documents still come from the retained scene-action pair document store.
StartResult start_from_queue(const game_scene_render_queue::Queue& queue, int variant);

scene_actions_active_playback::TickResult tick(
        scene_action_pair_document_store::Slot slot,
        float dt_seconds);

std::optional<scene_actions_active_playback::SlotState> snapshot(
        scene_action_pair_document_store::Slot slot);

// Submit the currently selected frame using the recovered initial klhuo system
// transforms. Missing/stale pixels remain retryable through the existing submit
// result and do not mutate the compositor.
scene_actions_render_submit::Result submit_current(
        scene_action_pair_document_store::Slot slot);

bool started();
int variant();
void reset();

}  // namespace nevergone::scene_actions_live_runtime
