#include "tutorial_enemy_roster.h"

#include <cmath>

namespace nevergone::tutorial_enemy_roster {
namespace {

constexpr float kDelayComparisonEpsilon = 0.000001f;

float due_time(const tutorial_enemy_spawn_plan::EnemyRequest& request) {
    return request.delayed ? request.delay_seconds : 0.0f;
}

}  // namespace

void Roster::reset() {
    plan_ = {};
    enemies_ = {};
    elapsed_seconds_ = 0.0f;
    ++generation_;  // Invalidate callbacks holding the prior wave token.
    active_ = false;
    completion_consumed_ = false;
}

bool Roster::begin(
        tutorial_enemy_spawn_plan::Kind kind,
        const tutorial_enemy_spawn_plan::Inputs& inputs) {
    const tutorial_enemy_spawn_plan::Plan plan =
        tutorial_enemy_spawn_plan::build(kind, inputs);
    if (plan.enemy_count == 0 || plan.enemy_count > enemies_.size()) {
        return false;
    }

    plan_ = plan;
    enemies_ = {};
    elapsed_seconds_ = 0.0f;
    ++generation_;
    active_ = true;
    completion_consumed_ = false;

    for (std::size_t i = 0; i < plan_.enemy_count; ++i) {
        enemies_[i].request = plan_.enemies[i];
        enemies_[i].planned = true;
    }
    return true;
}

bool Roster::advance_to(
        float elapsed_seconds,
        BaseHealthResolver resolver,
        std::uint64_t expected_generation,
        void* user_data) {
    if (!active_ || generation_ != expected_generation ||
        !std::isfinite(elapsed_seconds) || elapsed_seconds < 0.0f ||
        elapsed_seconds + kDelayComparisonEpsilon < elapsed_seconds_) {
        return false;
    }

    elapsed_seconds_ = elapsed_seconds;
    for (std::size_t i = 0; i < plan_.enemy_count; ++i) {
        EnemySlot& slot = enemies_[i];
        if (!slot.planned) continue;

        if (elapsed_seconds_ + kDelayComparisonEpsilon >= due_time(slot.request)) {
            slot.due = true;
        }
        if (!slot.due || slot.materialized || resolver == nullptr) continue;

        int base_health = 0;
        if (!resolver(slot.request.enemy_id, &base_health, user_data)) continue;
        if (!enemy_health_state::initialize(base_health, &slot.health)) continue;

        enemy_health_state::set_enemy_state(&slot.health, slot.request.enemy_state);
        slot.materialized = true;
    }
    return true;
}

bool Roster::apply_damage(
        std::size_t slot, float damage, std::uint64_t expected_generation) {
    if (!active_ || generation_ != expected_generation ||
        !std::isfinite(damage) || damage < 0.0f ||
        slot >= plan_.enemy_count || !enemies_[slot].materialized) {
        return false;
    }
    enemy_health_state::apply_beaten_damage(&enemies_[slot].health, damage);
    return true;
}

enemy_health_state::OnlineHealthUpdateResult Roster::apply_online_health_update(
        std::size_t slot,
        int update_type,
        int value,
        std::uint64_t expected_generation) {
    if (!active_ || generation_ != expected_generation ||
        slot >= plan_.enemy_count || !enemies_[slot].materialized) {
        return {};
    }
    return enemy_health_state::apply_online_health_update(
            &enemies_[slot].health, update_type, value);
}

bool Roster::set_dead(std::size_t slot, std::uint64_t expected_generation) {
    if (!active_ || generation_ != expected_generation ||
        slot >= plan_.enemy_count || !enemies_[slot].materialized) {
        return false;
    }
    enemy_health_state::set_dead(&enemies_[slot].health);
    return true;
}

bool Roster::set_enemy_state(
        std::size_t slot, int enemy_state, std::uint64_t expected_generation) {
    if (!active_ || generation_ != expected_generation ||
        slot >= plan_.enemy_count || !enemies_[slot].materialized) {
        return false;
    }
    enemy_health_state::set_enemy_state(&enemies_[slot].health, enemy_state);
    return true;
}

bool Roster::set_force_action_state_no_dead(
        std::size_t slot, bool enabled, std::uint64_t expected_generation) {
    if (!active_ || generation_ != expected_generation ||
        slot >= plan_.enemy_count || !enemies_[slot].materialized) {
        return false;
    }
    enemy_health_state::set_force_action_state_no_dead(&enemies_[slot].health, enabled);
    return true;
}

Snapshot Roster::snapshot() const {
    Snapshot out;
    out.kind = plan_.kind;
    out.enemies = enemies_;
    out.planned_count = plan_.enemy_count;
    out.elapsed_seconds = elapsed_seconds_;
    out.generation = generation_;
    out.active = active_;
    out.completion_consumed = completion_consumed_;

    for (std::size_t i = 0; i < plan_.enemy_count; ++i) {
        const EnemySlot& slot = enemies_[i];
        if (slot.due) ++out.due_count;
        if (slot.due && !slot.materialized) ++out.unresolved_health_count;
        if (!slot.materialized) continue;
        ++out.materialized_count;
        if (enemy_health_state::is_dead(slot.health)) {
            ++out.dead_count;
        } else if (enemy_health_state::is_low_health_percent(slot.health)) {
            ++out.low_health_count;
        }
    }

    out.all_materialized =
        out.planned_count > 0 && out.materialized_count == out.planned_count;
    out.all_defeated =
        out.all_materialized && out.dead_count == out.planned_count;
    return out;
}

bool Roster::consume_all_defeated(std::uint64_t expected_generation) {
    if (!active_ || generation_ != expected_generation || completion_consumed_) return false;
    if (!snapshot().all_defeated) return false;
    completion_consumed_ = true;
    active_ = false;
    return true;
}

}  // namespace nevergone::tutorial_enemy_roster
