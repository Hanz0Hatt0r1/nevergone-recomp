#include "tutorial_attack_live_gate.h"

#include <sstream>

namespace nevergone::tutorial_attack_live_gate {

void Gate::reset() {
    review_ = {};
    last_spawn_inputs_ = {};
    attack_.reset();
    enemy_record_count_ = 0;
    completion_publish_count_ = 0;
    ++generation_;
}

void Gate::set_review_bundle(const ReviewBundle& review) {
    review_ = review;
    ++generation_;
}

void Gate::set_player_face_dir_evidence(
        const tutorial_spawn_inputs::ReviewedPlayerFaceDir& player_face_dir) {
    if (review_.player_face_dir.reviewed == player_face_dir.reviewed &&
            review_.player_face_dir.control_generation == player_face_dir.control_generation &&
            review_.player_face_dir.value == player_face_dir.value) {
        return;
    }
    review_.player_face_dir = player_face_dir;
    ++generation_;
}

bool Gate::tick(
        std::uint64_t game_tick,
        const tutorial_gameplay_state::Snapshot& gameplay,
        const game_scene_player_spawn_state::Snapshot& spawn,
        const game_scene_player_motion_state::Snapshot& motion,
        const player_control_runtime_state::Snapshot& control,
        const game_levels_runtime_state::Snapshot& levels,
        const std::vector<game_levels_global_section::EnemyRecord>& enemy_records) {
    last_spawn_inputs_ = tutorial_spawn_inputs::resolve(
        spawn,
        motion,
        control,
        levels,
        review_.player_face_dir,
        review_.scene_limits);
    enemy_record_count_ = enemy_records.size();

    game_levels_enemy_record_mapping::ResolverContext resolver_context;
    resolver_context.mapping = &review_.enemy_mapping;
    resolver_context.records = &enemy_records;

    const bool mapping_usable =
        game_levels_enemy_record_mapping::is_usable(review_.enemy_mapping);
    const bool ok = attack_.tick(
        game_tick,
        gameplay,
        last_spawn_inputs_,
        mapping_usable
            ? &game_levels_enemy_record_mapping::roster_base_health_resolver
            : nullptr,
        mapping_usable ? &resolver_context : nullptr);
    ++generation_;
    return ok;
}

bool Gate::apply_damage(
        std::size_t slot,
        float damage,
        std::uint64_t expected_wave_generation) {
    const bool result = attack_.apply_damage(slot, damage, expected_wave_generation);
    if (result) ++generation_;
    return result;
}

bool Gate::set_dead(std::size_t slot, std::uint64_t expected_wave_generation) {
    const bool result = attack_.set_dead(slot, expected_wave_generation);
    if (result) ++generation_;
    return result;
}

bool Gate::set_enemy_state(
        std::size_t slot,
        int enemy_state,
        std::uint64_t expected_wave_generation) {
    const bool result = attack_.set_enemy_state(
        slot, enemy_state, expected_wave_generation);
    if (result) ++generation_;
    return result;
}

bool Gate::set_force_action_state_no_dead(
        std::size_t slot,
        bool enabled,
        std::uint64_t expected_wave_generation) {
    const bool result = attack_.set_force_action_state_no_dead(
        slot, enabled, expected_wave_generation);
    if (result) ++generation_;
    return result;
}

bool Gate::consume_completion_after_publish() {
    if (!attack_.consume_completion()) return false;
    ++completion_publish_count_;
    ++generation_;
    return true;
}

Snapshot Gate::snapshot() const {
    Snapshot out;
    out.generation = generation_;
    out.spawn_inputs = last_spawn_inputs_;
    out.attack = attack_.snapshot();
    out.enemy_mapping_usable =
        game_levels_enemy_record_mapping::is_usable(review_.enemy_mapping);
    out.enemy_record_count = enemy_record_count_;
    out.completion_publish_count = completion_publish_count_;
    return out;
}

std::string Gate::status_report() const {
    const Snapshot state = snapshot();
    std::ostringstream out;
    out << "Tutorial Attack live gate\n";
    out << "generation: " << state.generation << "\n";
    out << "spawn input status: "
        << tutorial_spawn_inputs::status_name(state.spawn_inputs.status) << "\n";
    out << "player face dir reviewed: "
        << (review_.player_face_dir.reviewed ? "yes" : "no") << "\n";
    out << "scene limits reviewed: "
        << (review_.scene_limits.reviewed ? "yes" : "no") << "\n";
    out << "enemy mapping reviewed: "
        << (review_.enemy_mapping.review_status ==
                    game_levels_enemy_record_mapping::ReviewStatus::kReviewed
                ? "yes" : "no")
        << "\n";
    out << "enemy mapping usable: " << (state.enemy_mapping_usable ? "yes" : "no") << "\n";
    out << "enemy records: " << state.enemy_record_count << "\n";
    out << "completion publishes: " << state.completion_publish_count << "\n";
    out << attack_.status_report();
    return out.str();
}

}  // namespace nevergone::tutorial_attack_live_gate
