#pragma once

#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <sstream>
#include <string>

#include "enemy_attack_value_contract.h"
#include "enemy_hitpoint_contract.h"
#include "game_levels_enemy_record_mapping.h"
#include "tutorial_attack_wave_runtime.h"

namespace nevergone::tutorial_enemy_actor_bindings {

struct Binding {
    bool active = false;
    std::size_t slot = 0;
    std::uint64_t wave_generation = 0;
    unsigned enemy_id = 0;
    int initial_enemy_state = 0;
    int current_enemy_state = 0;
    std::uint64_t state_started_game_tick = 0;
    bool state_changed = false;
    float base_health = 0.0f;
    std::uint32_t input_value_2bc = 0;
    float spawn_x = 0.0f;
    float spawn_y = 0.0f;
};

struct Snapshot {
    std::array<Binding, 3> bindings{};
    std::uint64_t generation = 0;
    std::uint64_t wave_generation = 0;
    std::size_t active_count = 0;
    bool wave_bound = false;
};

class Store {
public:
    void reset() {
        bindings_ = {};
        wave_generation_ = 0;
        wave_bound_ = false;
        ++generation_;
    }

    // Mirrors only roster slots that have actually materialized. It does not
    // create an EnemyObject, choose an asset, start AI, or invent combat state.
    // A new roster wave atomically invalidates all bindings from the old wave.
    bool sync(
            const tutorial_attack_wave_runtime::Snapshot& attack,
            std::uint64_t game_tick,
            const game_levels_enemy_record_mapping::Mapping* enemy_mapping = nullptr,
            const std::vector<game_levels_global_section::EnemyRecord>* enemy_records = nullptr) {
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
                // Materialization is monotonic within one wave. If a binding
                // was already published, its disappearance is inconsistent.
                if (bindings_[i].active) return false;
                continue;
            }

            if (!std::isfinite(slot.request.x) || !std::isfinite(slot.request.y) ||
                !std::isfinite(slot.health.max_health) || slot.health.max_health <= 0.0f) {
                return false;
            }
            if (bindings_[i].active) {
                Binding& binding = bindings_[i];
                if (binding.slot != i ||
                    binding.wave_generation != wave_generation_ ||
                    binding.enemy_id != slot.request.enemy_id ||
                    binding.initial_enemy_state != slot.request.enemy_state ||
                    binding.base_health != slot.health.max_health ||
                    binding.spawn_x != slot.request.x ||
                    binding.spawn_y != slot.request.y) {
                    return false;
                }
                if (binding.current_enemy_state != slot.health.enemy_state) {
                    binding.current_enemy_state = slot.health.enemy_state;
                    binding.state_started_game_tick = game_tick;
                    binding.state_changed = true;
                    changed = true;
                }
                continue;
            }

            if (enemy_mapping == nullptr || enemy_records == nullptr) return false;
            const auto input_value_2bc =
                game_levels_enemy_record_mapping::resolve_input_value_2bc(
                    *enemy_records, *enemy_mapping, slot.request.enemy_id);
            if (!input_value_2bc.has_value()) return false;

            Binding binding;
            binding.active = true;
            binding.slot = i;
            binding.wave_generation = wave_generation_;
            binding.enemy_id = slot.request.enemy_id;
            binding.initial_enemy_state = slot.request.enemy_state;
            binding.current_enemy_state = slot.health.enemy_state;
            binding.state_started_game_tick = game_tick;
            binding.base_health = slot.health.max_health;
            binding.input_value_2bc = *input_value_2bc;
            binding.spawn_x = slot.request.x;
            binding.spawn_y = slot.request.y;
            bindings_[i] = binding;
            changed = true;
        }

        if (changed) ++generation_;
        return true;
    }

    bool accepts(std::size_t slot, std::uint64_t expected_wave_generation) const {
        return wave_bound_ && expected_wave_generation != 0 &&
            expected_wave_generation == wave_generation_ &&
            slot < bindings_.size() && bindings_[slot].active &&
            bindings_[slot].wave_generation == expected_wave_generation;
    }

    std::optional<float> resolve_input_attack_value(
            std::size_t slot,
            std::int32_t current_frame,
            const enemy_actions_wbg_document::Document& action,
            std::uint64_t expected_wave_generation) const {
        if (!accepts(slot, expected_wave_generation)) return std::nullopt;
        return enemy_attack_value_contract::resolve_at_frame(
            bindings_[slot].input_value_2bc, action, current_frame);
    }

    std::optional<enemy_attack_value_contract::CombatFrameValues> resolve_combat_frame(
            std::size_t slot,
            std::int32_t current_frame,
            const enemy_actions_wbg_document::Document& action,
            std::uint64_t expected_wave_generation) const {
        if (!accepts(slot, expected_wave_generation)) return std::nullopt;
        return enemy_attack_value_contract::resolve_combat_frame(
            bindings_[slot].input_value_2bc, action, current_frame);
    }

    std::optional<enemy_hitpoint_contract::HitPoint> resolve_hitpoint(
            std::size_t slot,
            std::int32_t current_frame,
            std::int32_t frame_ordinal,
            const enemy_actions_wbg_document::Document& action,
            std::optional<enemy_hitpoint_contract::Point> sprite_bounds_origin,
            float enemy_offset_118,
            float enemy_offset_284,
            std::uint64_t expected_wave_generation) const {
        if (!accepts(slot, expected_wave_generation)) return std::nullopt;
        return enemy_hitpoint_contract::resolve(
            action, current_frame, frame_ordinal, sprite_bounds_origin,
            enemy_offset_118, enemy_offset_284);
    }

    Snapshot snapshot() const {
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

    std::string status_report() const {
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
                << " state=" << binding.current_enemy_state
                << " (initial=" << binding.initial_enemy_state << ")"
                << " hp=" << binding.base_health
                << " atk-base=" << binding.input_value_2bc
                << " x=" << binding.spawn_x
                << " y=" << binding.spawn_y << "\n";
        }
        return out.str();
    }

private:
    std::array<Binding, 3> bindings_{};
    std::uint64_t generation_ = 0;
    std::uint64_t wave_generation_ = 0;
    bool wave_bound_ = false;
};

}  // namespace nevergone::tutorial_enemy_actor_bindings
