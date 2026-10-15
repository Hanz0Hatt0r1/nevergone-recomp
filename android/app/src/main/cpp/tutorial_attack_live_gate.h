#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "game_levels_enemy_record_mapping.h"
#include "game_levels_runtime_state.h"
#include "game_scene_player_motion_state.h"
#include "game_scene_player_spawn_state.h"
#include "player_control_runtime_state.h"
#include "tutorial_attack_wave_runtime.h"
#include "tutorial_gameplay_state.h"
#include "tutorial_spawn_inputs.h"

namespace nevergone::tutorial_attack_live_gate {

struct ReviewBundle {
    tutorial_spawn_inputs::ReviewedPlayerFaceDir player_face_dir{};
    tutorial_spawn_inputs::ReviewedSceneLimits scene_limits{};
    game_levels_enemy_record_mapping::Mapping enemy_mapping{};
};

struct Snapshot {
    std::uint64_t generation = 0;
    tutorial_spawn_inputs::Result spawn_inputs{};
    tutorial_attack_wave_runtime::Snapshot attack{};
    bool enemy_mapping_usable = false;
    std::size_t enemy_record_count = 0;
    std::uint64_t completion_publish_count = 0;
};

class Gate {
public:
    void reset();
    void set_review_bundle(const ReviewBundle& review);

    // Updates only evidence that is regenerated from current live state.
    // Enemy-record mapping remains governed by its independent review path.
    void set_player_face_dir_evidence(
        const tutorial_spawn_inputs::ReviewedPlayerFaceDir& player_face_dir);
    void set_scene_limit_evidence(
        const tutorial_spawn_inputs::ReviewedSceneLimits& scene_limits);

    bool tick(
        std::uint64_t game_tick,
        const tutorial_gameplay_state::Snapshot& gameplay,
        const game_scene_player_spawn_state::Snapshot& spawn,
        const game_scene_player_motion_state::Snapshot& motion,
        const player_control_runtime_state::Snapshot& control,
        const game_levels_runtime_state::Snapshot& levels,
        const std::vector<game_levels_global_section::EnemyRecord>& enemy_records);

    bool apply_damage(
        std::size_t slot, float damage, std::uint64_t expected_wave_generation);
    bool set_dead(std::size_t slot, std::uint64_t expected_wave_generation);
    bool set_enemy_state(
        std::size_t slot, int enemy_state, std::uint64_t expected_wave_generation);
    bool set_force_action_state_no_dead(
        std::size_t slot, bool enabled, std::uint64_t expected_wave_generation);

    // Consume only after the caller has successfully published
    // kTutorialEnemiesDefeated to tutorial_gameplay_state.
    bool consume_completion_after_publish();

    Snapshot snapshot() const;
    std::string status_report() const;

private:
    ReviewBundle review_{};
    tutorial_spawn_inputs::Result last_spawn_inputs_{};
    tutorial_attack_wave_runtime::Runtime attack_{};
    std::uint64_t generation_ = 0;
    std::size_t enemy_record_count_ = 0;
    std::uint64_t completion_publish_count_ = 0;
    std::uint64_t bound_load_attempt_ = 0;
    std::string bound_scene_guid_{};
};

// Android frame-loop instance. Current player facing and scene-limit providers
// install only values independently proven from the imported state; unresolved
// enemy mapping remains fail-closed.
void live_reset();
void live_set_review_bundle(const ReviewBundle& review);
bool live_tick();
bool live_apply_damage(
    std::size_t slot, float damage, std::uint64_t expected_wave_generation);
bool live_set_dead(std::size_t slot, std::uint64_t expected_wave_generation);
bool live_set_enemy_state(
    std::size_t slot, int enemy_state, std::uint64_t expected_wave_generation);
bool live_set_force_action_state_no_dead(
    std::size_t slot, bool enabled, std::uint64_t expected_wave_generation);
Snapshot live_snapshot();
std::string live_status_report();

}  // namespace nevergone::tutorial_attack_live_gate
