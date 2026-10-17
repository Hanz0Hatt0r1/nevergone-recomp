#include "tutorial_attack_live_gate.h"

#include <mutex>

#include "game_clock.h"
#include "tutorial_player_face_dir_provider.h"
#include "tutorial_scene_limit_provider.h"

namespace nevergone::tutorial_attack_live_gate {
namespace {

std::mutex g_live_mutex;
Gate g_live_gate;
tutorial_player_face_dir_provider::Resolution g_last_face_dir_resolution;
tutorial_scene_limit_provider::Resolution g_last_scene_limit_resolution;

}  // namespace

void live_reset() {
    std::lock_guard<std::mutex> lock(g_live_mutex);
    g_live_gate.reset();
    g_last_face_dir_resolution = {};
    g_last_scene_limit_resolution = {};
}

void live_set_review_bundle(const ReviewBundle& review) {
    std::lock_guard<std::mutex> lock(g_live_mutex);
    g_live_gate.set_review_bundle(review);
}

bool live_tick() {
    const auto gameplay = tutorial_gameplay_state::snapshot();
    const auto spawn = game_scene_player_spawn_state::snapshot();
    const auto motion = game_scene_player_motion_state::snapshot();
    const auto control = player_control_runtime_state::snapshot();
    const auto levels = game_levels_runtime_state::snapshot();
    const auto port_node = game_levels_runtime_state::current_port_node_record();
    const auto enemy_records = game_levels_runtime_state::current_global_enemy_records();
    const std::uint64_t game_tick = game_clock::tick_count();
    const auto face_dir = tutorial_player_face_dir_provider::resolve(control);
    const auto scene_limits = tutorial_scene_limit_provider::resolve(levels, port_node);
    const auto enemy_mapping =
        game_levels_enemy_record_mapping::reviewed_original_mapping();

    std::lock_guard<std::mutex> lock(g_live_mutex);
    g_last_face_dir_resolution = face_dir;
    g_last_scene_limit_resolution = scene_limits;
    g_live_gate.set_player_face_dir_evidence(face_dir.face_dir);
    g_live_gate.set_scene_limit_evidence(scene_limits.scene_limits);
    g_live_gate.set_enemy_mapping_evidence(enemy_mapping);
    if (!g_live_gate.tick(
            game_tick,
            gameplay,
            spawn,
            motion,
            control,
            levels,
            enemy_records)) {
        return false;
    }

    if (!g_live_gate.snapshot().attack.completion_pending) return true;
    if (!tutorial_gameplay_state::observe(
            tutorial_gameplay_state::Event::kTutorialEnemiesDefeated)) {
        return false;
    }
    return g_live_gate.consume_completion_after_publish();
}

bool live_apply_damage(
        std::size_t slot,
        float damage,
        std::uint64_t expected_wave_generation) {
    std::lock_guard<std::mutex> lock(g_live_mutex);
    return g_live_gate.apply_damage(slot, damage, expected_wave_generation);
}

bool live_set_dead(std::size_t slot, std::uint64_t expected_wave_generation) {
    std::lock_guard<std::mutex> lock(g_live_mutex);
    return g_live_gate.set_dead(slot, expected_wave_generation);
}

bool live_set_enemy_state(
        std::size_t slot,
        int enemy_state,
        std::uint64_t expected_wave_generation) {
    std::lock_guard<std::mutex> lock(g_live_mutex);
    return g_live_gate.set_enemy_state(slot, enemy_state, expected_wave_generation);
}

bool live_set_force_action_state_no_dead(
        std::size_t slot,
        bool enabled,
        std::uint64_t expected_wave_generation) {
    std::lock_guard<std::mutex> lock(g_live_mutex);
    return g_live_gate.set_force_action_state_no_dead(
        slot, enabled, expected_wave_generation);
}

std::optional<float> live_resolve_input_attack_value(
        std::size_t slot,
        std::int32_t current_frame,
        const enemy_actions_wbg_document::Document& action,
        std::uint64_t expected_wave_generation) {
    std::lock_guard<std::mutex> lock(g_live_mutex);
    return g_live_gate.resolve_input_attack_value(
        slot, current_frame, action, expected_wave_generation);
}

Snapshot live_snapshot() {
    std::lock_guard<std::mutex> lock(g_live_mutex);
    return g_live_gate.snapshot();
}

std::string live_status_report() {
    std::lock_guard<std::mutex> lock(g_live_mutex);
    std::string report = g_live_gate.status_report();
    report += tutorial_player_face_dir_provider::status_report(
        g_last_face_dir_resolution);
    report += tutorial_scene_limit_provider::status_report(
        g_last_scene_limit_resolution);
    return report;
}

}  // namespace nevergone::tutorial_attack_live_gate
