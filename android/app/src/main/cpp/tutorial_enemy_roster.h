#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

#include "enemy_health_state.h"
#include "tutorial_enemy_spawn_plan.h"

namespace nevergone::tutorial_enemy_roster {

using BaseHealthResolver = bool (*)(
        unsigned enemy_id,
        int* out_base_health,
        void* user_data);

struct EnemySlot {
    tutorial_enemy_spawn_plan::EnemyRequest request{};
    enemy_health_state::State health{};
    bool planned = false;
    bool due = false;
    bool materialized = false;
};

struct Snapshot {
    tutorial_enemy_spawn_plan::Kind kind =
        tutorial_enemy_spawn_plan::Kind::kAttackWave;
    std::array<EnemySlot, 3> enemies{};
    std::size_t planned_count = 0;
    std::size_t due_count = 0;
    std::size_t materialized_count = 0;
    std::size_t dead_count = 0;
    std::size_t unresolved_health_count = 0;
    float elapsed_seconds = 0.0f;
    std::uint64_t generation = 0;
    bool active = false;
    bool all_materialized = false;
    bool all_defeated = false;
    bool completion_consumed = false;
};

class Roster {
public:
    void reset();

    // Starts one recovered LessonLayer enemy plan. This does not invent enemy
    // HP: slots become materialized only after advance_to() obtains a positive
    // base-health value from the imported-data resolver.
    bool begin(
            tutorial_enemy_spawn_plan::Kind kind,
            const tutorial_enemy_spawn_plan::Inputs& inputs);

    // Advances against absolute elapsed time since begin(). Delayed requests
    // use the exact delay_seconds values recovered in tutorial_enemy_spawn_plan.
    // A due request whose GameLevelsEnemyData health is not available remains
    // pending and is retried on later calls.
    bool advance_to(
            float elapsed_seconds,
            BaseHealthResolver resolver,
            void* user_data = nullptr);

    bool apply_damage(std::size_t slot, float damage);
    bool set_dead(std::size_t slot);
    bool set_enemy_state(std::size_t slot, int enemy_state);
    bool set_force_action_state_no_dead(std::size_t slot, bool enabled);

    Snapshot snapshot() const;

    // One-shot edge used by the LessonLayer integration. It cannot fire until
    // every planned request has materialized and every corresponding recovered
    // EnemyObject death predicate is true.
    bool consume_all_defeated();

private:
    tutorial_enemy_spawn_plan::Plan plan_{};
    std::array<EnemySlot, 3> enemies_{};
    float elapsed_seconds_ = 0.0f;
    std::uint64_t generation_ = 0;
    bool active_ = false;
    bool completion_consumed_ = false;
};

}  // namespace nevergone::tutorial_enemy_roster
