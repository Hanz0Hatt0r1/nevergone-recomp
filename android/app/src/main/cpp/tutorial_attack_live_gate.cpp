#include "tutorial_attack_live_gate.h"

#include <sstream>

namespace nevergone::tutorial_attack_live_gate {
namespace {

bool same_scene_limits(
        const tutorial_spawn_inputs::ReviewedSceneLimits& a,
        const tutorial_spawn_inputs::ReviewedSceneLimits& b) {
    return a.reviewed == b.reviewed &&
        a.runtime_load_attempt_count == b.runtime_load_attempt_count &&
        a.scene_guid == b.scene_guid &&
        a.vertical_edge_a == b.vertical_edge_a &&
        a.vertical_edge_b == b.vertical_edge_b &&
        a.right_edge_x == b.right_edge_x;
}

bool same_enemy_mapping(
        const game_levels_enemy_record_mapping::Mapping& a,
        const game_levels_enemy_record_mapping::Mapping& b) {
    return a.identity_field == b.identity_field &&
        a.base_health_field == b.base_health_field &&
        a.review_status == b.review_status;
}

}  // namespace

void Gate::invalidate_wave() {
    if (!attack_.snapshot().wave_started &&
            enemy_bindings_.snapshot().active_count == 0) return;
    attack_.reset();
    enemy_bindings_.reset();
    bound_load_attempt_ = 0;
    bound_scene_guid_.clear();
    ++generation_;
}

void Gate::reset() {
    review_ = {};
    last_spawn_inputs_ = {};
    attack_.reset();
    enemy_bindings_.reset();
    enemy_record_count_ = 0;
    completion_publish_count_ = 0;
    bound_load_attempt_ = 0;
    bound_scene_guid_.clear();
    ++generation_;
}

void Gate::set_review_bundle(const ReviewBundle& review) {
    if (!same_scene_limits(review_.scene_limits, review.scene_limits) ||
            !same_enemy_mapping(review_.enemy_mapping, review.enemy_mapping)) {
        invalidate_wave();
    }
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

void Gate::set_scene_limit_evidence(
        const tutorial_spawn_inputs::ReviewedSceneLimits& scene_limits) {
    if (same_scene_limits(review_.scene_limits, scene_limits)) return;
    invalidate_wave();
    review_.scene_limits = scene_limits;
    ++generation_;
}

void Gate::set_enemy_mapping_evidence(
        const game_levels_enemy_record_mapping::Mapping& enemy_mapping) {
    if (same_enemy_mapping(review_.enemy_mapping, enemy_mapping)) return;
    invalidate_wave();
    review_.enemy_mapping = enemy_mapping;
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

    // A wave belongs to one imported scene/load attempt. Reject callbacks
    // from an old scene even if the gameplay step is still Attack.
    if (bound_load_attempt_ != 0 &&
            (levels.load_attempt_count != bound_load_attempt_ ||
             levels.current_scene_guid != bound_scene_guid_ ||
             !levels.current_scene_render_queue_ready ||
             levels.status != game_levels_runtime_state::LoadStatus::kReady)) {
        attack_.reset();
        bound_load_attempt_ = 0;
        bound_scene_guid_.clear();
        ++generation_;
    }

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
    const auto attack_state = attack_.snapshot();

    // Publish actor identities only after the underlying roster slots have
    // materialized with imported HP. This boundary is intentionally earlier
    // than sprite/AI creation and later than the recovered spawn plan.
    if (!enemy_bindings_.sync(attack_state)) return false;

    if (attack_state.wave_started && bound_load_attempt_ == 0) {
        bound_load_attempt_ = levels.load_attempt_count;
        bound_scene_guid_ = levels.current_scene_guid;
    } else if (!attack_state.wave_started) {
        bound_load_attempt_ = 0;
        bound_scene_guid_.clear();
    }
    ++generation_;
    return ok;
}

bool Gate::apply_damage(
        std::size_t slot,
        float damage,
        std::uint64_t expected_wave_generation) {
    if (!enemy_bindings_.accepts(slot, expected_wave_generation)) return false;
    const bool result = attack_.apply_damage(slot, damage, expected_wave_generation);
    if (result) ++generation_;
    return result;
}

bool Gate::set_dead(std::size_t slot, std::uint64_t expected_wave_generation) {
    if (!enemy_bindings_.accepts(slot, expected_wave_generation)) return false;
    const bool result = attack_.set_dead(slot, expected_wave_generation);
    if (result) ++generation_;
    return result;
}

bool Gate::set_enemy_state(
        std::size_t slot,
        int enemy_state,
        std::uint64_t expected_wave_generation) {
    if (!enemy_bindings_.accepts(slot, expected_wave_generation)) return false;
    const bool result = attack_.set_enemy_state(
        slot, enemy_state, expected_wave_generation);
    if (result) ++generation_;
    return result;
}

bool Gate::set_force_action_state_no_dead(
        std::size_t slot,
        bool enabled,
        std::uint64_t expected_wave_generation) {
    if (!enemy_bindings_.accepts(slot, expected_wave_generation)) return false;
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
    out.enemy_bindings = enemy_bindings_.snapshot();
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
    out << "enemy mapping identity field: "
        << game_levels_enemy_record_mapping::field_name(review_.enemy_mapping.identity_field)
        << "\n";
    out << "enemy mapping base-health field: "
        << game_levels_enemy_record_mapping::field_name(review_.enemy_mapping.base_health_field)
        << "\n";
    out << "enemy mapping usable: " << (state.enemy_mapping_usable ? "yes" : "no") << "\n";
    out << "enemy records: " << state.enemy_record_count << "\n";
    out << "completion publishes: " << state.completion_publish_count << "\n";
    out << attack_.status_report();
    out << enemy_bindings_.status_report();
    return out.str();
}

}  // namespace nevergone::tutorial_attack_live_gate
