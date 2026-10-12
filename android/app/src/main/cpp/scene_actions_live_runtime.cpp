#include "scene_actions_live_runtime.h"

#include <mutex>

#include "game_levels_runtime_state.h"

namespace nevergone::scene_actions_live_runtime {
namespace {

std::mutex g_mutex;
scene_actions_runtime_controller::Controller g_controller;

CurrentSceneStatus map_current_binding(scene_actions_current_scene_binding::Status status) {
    switch (status) {
        case scene_actions_current_scene_binding::Status::kReady:
            return CurrentSceneStatus::kReady;
        case scene_actions_current_scene_binding::Status::kNoPair:
            return CurrentSceneStatus::kNoPair;
        case scene_actions_current_scene_binding::Status::kAmbiguousPairs:
            return CurrentSceneStatus::kAmbiguousPairs;
        case scene_actions_current_scene_binding::Status::kInvalidPair:
            return CurrentSceneStatus::kInvalidPair;
    }
    return CurrentSceneStatus::kInvalidPair;
}

StartStatus map_binding(scene_actions_current_scene_binding::Status status) {
    switch (status) {
        case scene_actions_current_scene_binding::Status::kNoPair:
            return StartStatus::kNoPair;
        case scene_actions_current_scene_binding::Status::kAmbiguousPairs:
            return StartStatus::kAmbiguousPairs;
        case scene_actions_current_scene_binding::Status::kInvalidPair:
            return StartStatus::kInvalidPair;
        case scene_actions_current_scene_binding::Status::kReady:
            break;
    }
    return StartStatus::kControllerRejected;
}

StartStatus map_controller(scene_actions_runtime_controller::StartStatus status) {
    switch (status) {
        case scene_actions_runtime_controller::StartStatus::kStarted:
            return StartStatus::kStarted;
        case scene_actions_runtime_controller::StartStatus::kAlreadyConsumed:
            return StartStatus::kAlreadyConsumed;
        case scene_actions_runtime_controller::StartStatus::kPairUnavailable:
            return StartStatus::kPairUnavailable;
        case scene_actions_runtime_controller::StartStatus::kInvalidPlan:
            return StartStatus::kControllerRejected;
    }
    return StartStatus::kControllerRejected;
}

}  // namespace

CurrentSceneStatus current_scene_status() {
    const auto queue = game_levels_runtime_state::current_scene_render_queue();
    if (!queue.has_value()) return CurrentSceneStatus::kSceneUnavailable;
    return map_current_binding(scene_actions_current_scene_binding::resolve(*queue).status);
}

StartResult start_current_scene(int requested_variant) {
    const auto queue = game_levels_runtime_state::current_scene_render_queue();
    if (!queue.has_value()) {
        StartResult out;
        out.status = StartStatus::kSceneUnavailable;
        out.variant = requested_variant;
        return out;
    }
    return start_from_queue(*queue, requested_variant);
}

StartResult start_from_queue(const game_scene_render_queue::Queue& queue, int requested_variant) {
    std::lock_guard<std::mutex> lock(g_mutex);

    StartResult out;
    out.variant = requested_variant;
    if (g_controller.started()) {
        out.status = StartStatus::kAlreadyStarted;
        return out;
    }

    const auto binding = scene_actions_current_scene_binding::resolve(queue);
    out.binding_status = binding.status;
    if (binding.status != scene_actions_current_scene_binding::Status::kReady ||
            !binding.plan.has_value()) {
        out.status = map_binding(binding.status);
        return out;
    }

    const auto started = g_controller.start_from_store(*binding.plan, requested_variant);
    out.controller_status = started.status;
    out.variant = started.variant;
    out.status = map_controller(started.status);
    return out;
}

scene_actions_active_playback::TickResult tick(
        scene_action_pair_document_store::Slot slot,
        float dt_seconds) {
    std::lock_guard<std::mutex> lock(g_mutex);
    return g_controller.tick(slot, dt_seconds);
}

std::optional<scene_actions_active_playback::SlotState> snapshot(
        scene_action_pair_document_store::Slot slot) {
    std::lock_guard<std::mutex> lock(g_mutex);
    return g_controller.snapshot(slot);
}

scene_actions_render_submit::Result submit_current(
        scene_action_pair_document_store::Slot slot) {
    std::lock_guard<std::mutex> lock(g_mutex);
    const auto state = g_controller.snapshot(slot);
    if (!state.has_value()) return {};
    return scene_actions_render_submit::submit_initial_klhuo(*state);
}

bool started() {
    std::lock_guard<std::mutex> lock(g_mutex);
    return g_controller.started();
}

int variant() {
    std::lock_guard<std::mutex> lock(g_mutex);
    return g_controller.variant();
}

void reset() {
    std::lock_guard<std::mutex> lock(g_mutex);
    g_controller.reset();
}

}  // namespace nevergone::scene_actions_live_runtime
