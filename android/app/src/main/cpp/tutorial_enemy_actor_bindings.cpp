#include "tutorial_enemy_actor_bindings.h"

#include <cmath>
#include <sstream>

namespace nevergone::tutorial_enemy_actor_bindings {
namespace {

bool finite_binding_source(const tutorial_enemy_roster::EnemySlot& slot) {
    return std::isfinite(slot.request.x) && std::isfinite(slot.request.y) &&
        std::isfinite(slot.health.max_health) && slot.health.max_health > 0.0f;
}

bool same_binding(const Binding& binding,
                  const tutorial_enemy_roster::EnemySlot& slot,
                  std::size_t index,
                  std::uint64_t wave_generation) {
    return binding.active &&
        binding.slot == index &&
        binding.wave_generation == wave_generation &&
        binding.enemy_id == slot.request.enemy_id &&
        binding.initial_enemy_state == slot.request.enemy_state &&
        binding.base_health == slot.health.max_health &&
        binding.spawn_x == slot.request.x &&
        binding.spawn_y == slot.request.y;
}

}  // namespace

void Store::reset() {
    bindings_ = {};
    wave_generation_ = 0;
    wave_bound_ = false;
    ++generation_;
}

bool Store::sync(const tutorial_attack_wave_runtime::Snapshot& attack) {
    if (!attack.wave_started) {
        if (wave_bound_ || wave_generation_ != 0) reset();
        return true;
    }

    if (attack.wave_generation == 0 ||
        attack.roster.generation != attack.wave_generation ||
        attack.roster.planned_count == 0 ||
        attack.roster.planned_count > bindings_.size()) {
        return false;
    }

    bool changed = false;
    if (!wave_bound_ || wave_generation_ != attack.wave_generation) {
        bindings_ = {};
        wave_generation_ = attack.wave_generation;
        wave_bound_ = true;
        changed = true;
    }

    for (std::size_t i = 0; i < bindings_.size(); ++i) {
        const bool planned = i < attack.roster.planned_count;
        if (!planned) {
            if (bindings_[i].active) return false;
            continue;
        }

        const auto& slot = attack.roster.enemies[i];
        if (!slot.planned) return false;

        if (!slot.materialized) {
            // Materialization is monotonic within one wave. If a binding was
            // already published for this slot, its disappearance would mean
            // callers could retain an identity that no longer exists.
            if (bindings_[i].active) return false;
            continue;
        }

        if (!finite_binding_source(slot)) return false;
        if (bindings_[i].active) {
            if (!same_binding(bindings_[i], slot, i, wave_generation_)) return false;
            continue;
        }

        Binding binding;
        binding.active = true;
        binding.slot = i;
        binding.wave_generation = wave_generation_;
        binding.enemy_id = slot.request.enemy_id;
        binding.initial_enemy_state = slot.request.enemy_state;
        binding.base_health = slot.health.max_health;
        binding.spawn_x = slot.request.x;
        binding.spawn_y = slot.request.y;
        bindings_[i] = binding;
        changed = true;
    }

    if (changed) ++generation_;
    return true;
}

bool Store::accepts(
        std::size_t slot,
        std::uint64_t expected_wave_generation) const {
    return wave_bound_ && expected_wave_generation != 0 &&
        expected_wave_generation == wave_generation_ &&
        slot < bindings_.size() && bindings_[slot].active &&
        bindings_[slot].wave_generation == expected_wave_generation;
}

Snapshot Store::snapshot() const {
    Snapshot out;
    out.bindings = bindings_;
    out.generation = generation_;
    out.wave_generation = wave_generation_;
    out.wave_bound = wave_bound_;
    for (const auto& binding : bindings_) {
        if (binding.active) ++out.active_count;
    }
    return out;
}

std::string Store::status_report() const {
    const Snapshot state = snapshot();
    std::ostringstream out;
    out << "Tutorial enemy actor bindings\n";
    out << "generation: " << state.generation << "\n";
    out << "wave bound: " << (state.wave_bound ? "yes" : "no") << "\n";
    out << "wave generation: " << state.wave_generation << "\n";
    out << "active bindings: " << state.active_count << "\n";
    for (const auto& binding : state.bindings) {
        if (!binding.active) continue;
        out << "binding slot " << binding.slot
            << ": enemy=" << binding.enemy_id
            << " state=" << binding.initial_enemy_state
            << " hp=" << binding.base_health
            << " x=" << binding.spawn_x
            << " y=" << binding.spawn_y << "\n";
    }
    return out.str();
}

}  // namespace nevergone::tutorial_enemy_actor_bindings
