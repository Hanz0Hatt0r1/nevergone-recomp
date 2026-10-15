#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

#include "tutorial_enemy_roster.h"
#include "tutorial_gameplay_state.h"
#include "tutorial_spawn_inputs.h"

namespace nevergone::tutorial_attack_wave_runtime {

enum class Status {
    kIdle = 0,
    kWaitingForSpawnInputs,
    kWaitingForBaseHealth,
    kPlanInvalid,
    kActive,
    kClockRegression,
    kCompletionPending,
    kCompleted,
};

struct Snapshot {
    Status status = Status::kIdle;
    std::uint64_t generation = 0;
    std::uint64_t wave_generation = 0;
    std::uint64_t start_game_tick = 0;
    std::uint64_t last_game_tick = 0;
    bool wave_started = false;
    bool wave_completed = false;
    bool completion_pending = false;
    tutorial_enemy_roster::Snapshot roster{};
};

class Runtime {
public:
    void reset();

    // Advances only the first recovered Attack lesson wave. The wave is not
    // begun until both the generation-checked spawn inputs and all base-health
    // values required by the recovered plan resolve. This prevents freezing
    // player-relative spawn geometry while HP evidence is still unavailable.
    bool tick(
        std::uint64_t game_tick,
        const tutorial_gameplay_state::Snapshot& gameplay,
        const tutorial_spawn_inputs::Result& spawn_inputs,
        tutorial_enemy_roster::BaseHealthResolver resolver,
        void* resolver_user_data = nullptr);

    // Enemy callbacks must carry the wave generation captured when their slot
    // was materialized. A slot index alone is not stable across tutorial waves.
    bool apply_damage(
        std::size_t slot, float damage, std::uint64_t expected_wave_generation);
    bool set_dead(std::size_t slot, std::uint64_t expected_wave_generation);
    bool set_enemy_state(
        std::size_t slot, int enemy_state, std::uint64_t expected_wave_generation);
    bool set_force_action_state_no_dead(
        std::size_t slot, bool enabled, std::uint64_t expected_wave_generation);

    // One-shot edge for the outer LessonLayer integration. Calling this never
    // mutates tutorial_gameplay_state directly; the frame boundary may publish
    // kTutorialEnemiesDefeated after consuming the edge. Completion remains
    // latched until the gameplay step actually leaves Attack.
    bool consume_completion();

    Snapshot snapshot() const;
    std::string status_report() const;

private:
    bool required_base_health_ready(
        const tutorial_enemy_spawn_plan::Plan& plan,
        tutorial_enemy_roster::BaseHealthResolver resolver,
        void* resolver_user_data) const;
    void set_status(Status status);

    tutorial_enemy_roster::Roster roster_{};
    Status status_ = Status::kIdle;
    std::uint64_t generation_ = 0;
    std::uint64_t wave_generation_ = 0;
    std::uint64_t start_game_tick_ = 0;
    std::uint64_t last_game_tick_ = 0;
    bool wave_started_ = false;
    bool wave_completed_ = false;
    bool completion_pending_ = false;
};

const char* status_name(Status status);

}  // namespace nevergone::tutorial_attack_wave_runtime
