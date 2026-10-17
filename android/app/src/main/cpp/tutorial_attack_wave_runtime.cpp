#include "tutorial_attack_wave_runtime.h"

#include <array>
#include <cmath>
#include <sstream>

#include "game_clock.h"

namespace nevergone::tutorial_attack_wave_runtime {

void Runtime::set_status(Status status) {
    if (status_ == status) return;
    status_ = status;
    ++generation_;
}

void Runtime::reset() {
    roster_.reset();
    wave_generation_ = 0;
    start_game_tick_ = 0;
    last_game_tick_ = 0;
    wave_started_ = false;
    wave_completed_ = false;
    completion_pending_ = false;
    set_status(Status::kIdle);
}

bool Runtime::required_base_health_ready(
        const tutorial_enemy_spawn_plan::Plan& plan,
        tutorial_enemy_roster::BaseHealthResolver resolver,
        void* resolver_user_data) const {
    if (resolver == nullptr || plan.enemy_count == 0 ||
            plan.enemy_count > plan.enemies.size()) {
        return false;
    }

    std::array<unsigned, 3> seen_ids{};
    std::size_t seen_count = 0;
    for (std::size_t i = 0; i < plan.enemy_count; ++i) {
        const unsigned enemy_id = plan.enemies[i].enemy_id;
        bool already_seen = false;
        for (std::size_t j = 0; j < seen_count; ++j) {
            if (seen_ids[j] == enemy_id) {
                already_seen = true;
                break;
            }
        }
        if (already_seen) continue;

        int base_health = 0;
        if (!resolver(enemy_id, &base_health, resolver_user_data) || base_health <= 0) {
            return false;
        }
        if (seen_count < seen_ids.size()) {
            seen_ids[seen_count++] = enemy_id;
        }
    }
    return true;
}

bool Runtime::tick(
        std::uint64_t game_tick,
        const tutorial_gameplay_state::Snapshot& gameplay,
        const tutorial_spawn_inputs::Result& spawn_inputs,
        tutorial_enemy_roster::BaseHealthResolver resolver,
        void* resolver_user_data) {
    if (gameplay.step != tutorial_gameplay_state::Step::kAttack) {
        if (wave_started_ || wave_completed_ || completion_pending_ ||
                status_ != Status::kIdle) {
            reset();
        }
        return true;
    }

    if (wave_completed_) {
        last_game_tick_ = game_tick;
        set_status(completion_pending_ ? Status::kCompletionPending : Status::kCompleted);
        return true;
    }

    if (!wave_started_) {
        if (!spawn_inputs.ready()) {
            set_status(Status::kWaitingForSpawnInputs);
            return true;
        }

        const tutorial_enemy_spawn_plan::Plan plan = tutorial_enemy_spawn_plan::build(
            tutorial_enemy_spawn_plan::Kind::kAttackWave,
            spawn_inputs.inputs);
        if (plan.enemy_count == 0 || plan.enemy_count > plan.enemies.size()) {
            set_status(Status::kPlanInvalid);
            return false;
        }
        if (!required_base_health_ready(plan, resolver, resolver_user_data)) {
            set_status(Status::kWaitingForBaseHealth);
            return true;
        }
        if (!roster_.begin(
                tutorial_enemy_spawn_plan::Kind::kAttackWave,
                spawn_inputs.inputs)) {
            set_status(Status::kPlanInvalid);
            return false;
        }

        wave_generation_ = roster_.snapshot().generation;
        if (wave_generation_ == 0) {
            set_status(Status::kPlanInvalid);
            return false;
        }
        start_game_tick_ = game_tick;
        last_game_tick_ = game_tick;
        wave_started_ = true;
        ++generation_;
        set_status(Status::kActive);
    } else if (game_tick < last_game_tick_ || game_tick < start_game_tick_) {
        set_status(Status::kClockRegression);
        return false;
    }

    last_game_tick_ = game_tick;
    const double elapsed = static_cast<double>(game_tick - start_game_tick_) *
        game_clock::kFixedStepSeconds;
    if (!std::isfinite(elapsed) || elapsed < 0.0) {
        set_status(Status::kClockRegression);
        return false;
    }
    if (!roster_.advance_to(
            static_cast<float>(elapsed), resolver, wave_generation_, resolver_user_data)) {
        return false;
    }

    if (roster_.consume_all_defeated(wave_generation_)) {
        wave_completed_ = true;
        completion_pending_ = true;
        ++generation_;
        set_status(Status::kCompletionPending);
    } else {
        set_status(Status::kActive);
    }
    return true;
}

bool Runtime::apply_damage(
        std::size_t slot,
        float damage,
        std::uint64_t expected_wave_generation) {
    if (wave_completed_ || !wave_started_ ||
            expected_wave_generation != wave_generation_) {
        return false;
    }
    const bool applied = roster_.apply_damage(slot, damage, expected_wave_generation);
    if (applied) ++generation_;
    return applied;
}

bool Runtime::set_dead(std::size_t slot, std::uint64_t expected_wave_generation) {
    if (wave_completed_ || !wave_started_ ||
            expected_wave_generation != wave_generation_) {
        return false;
    }
    const bool applied = roster_.set_dead(slot, expected_wave_generation);
    if (applied) ++generation_;
    return applied;
}

bool Runtime::set_enemy_state(
        std::size_t slot,
        int enemy_state,
        std::uint64_t expected_wave_generation) {
    if (wave_completed_ || !wave_started_ ||
            expected_wave_generation != wave_generation_) {
        return false;
    }
    const bool applied = roster_.set_enemy_state(
        slot, enemy_state, expected_wave_generation);
    if (applied) ++generation_;
    return applied;
}

bool Runtime::set_force_action_state_no_dead(
        std::size_t slot,
        bool enabled,
        std::uint64_t expected_wave_generation) {
    if (wave_completed_ || !wave_started_ ||
            expected_wave_generation != wave_generation_) {
        return false;
    }
    const bool applied = roster_.set_force_action_state_no_dead(
        slot, enabled, expected_wave_generation);
    if (applied) ++generation_;
    return applied;
}

bool Runtime::consume_completion() {
    if (!completion_pending_) return false;
    completion_pending_ = false;
    ++generation_;
    set_status(Status::kCompleted);
    return true;
}

Snapshot Runtime::snapshot() const {
    Snapshot out;
    out.status = status_;
    out.generation = generation_;
    out.wave_generation = wave_generation_;
    out.start_game_tick = start_game_tick_;
    out.last_game_tick = last_game_tick_;
    out.wave_started = wave_started_;
    out.wave_completed = wave_completed_;
    out.completion_pending = completion_pending_;
    out.roster = roster_.snapshot();
    return out;
}

std::string Runtime::status_report() const {
    const Snapshot state = snapshot();
    std::ostringstream out;
    out << "Tutorial attack wave runtime\n";
    out << "status: " << status_name(state.status) << "\n";
    out << "generation: " << state.generation << "\n";
    out << "wave generation: " << state.wave_generation << "\n";
    out << "start game tick: " << state.start_game_tick << "\n";
    out << "last game tick: " << state.last_game_tick << "\n";
    out << "wave started: " << (state.wave_started ? "true" : "false") << "\n";
    out << "wave completed: " << (state.wave_completed ? "true" : "false") << "\n";
    out << "completion pending: " << (state.completion_pending ? "true" : "false") << "\n";
    out << "planned enemies: " << state.roster.planned_count << "\n";
    out << "materialized enemies: " << state.roster.materialized_count << "\n";
    out << "dead enemies: " << state.roster.dead_count << "\n";
    out << "low-health enemies: " << state.roster.low_health_count << "\n";
    out << "unresolved health: " << state.roster.unresolved_health_count << "\n";
    return out.str();
}

const char* status_name(Status status) {
    switch (status) {
        case Status::kIdle: return "idle";
        case Status::kWaitingForSpawnInputs: return "waiting-for-spawn-inputs";
        case Status::kWaitingForBaseHealth: return "waiting-for-base-health";
        case Status::kPlanInvalid: return "plan-invalid";
        case Status::kActive: return "active";
        case Status::kClockRegression: return "clock-regression";
        case Status::kCompletionPending: return "completion-pending";
        case Status::kCompleted: return "completed";
    }
    return "unknown";
}

}  // namespace nevergone::tutorial_attack_wave_runtime
