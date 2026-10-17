#include <cassert>

#include "tutorial_enemy_action_animation.h"

namespace {

nevergone::enemy_actions_wbg_document::Document clip(
        std::initializer_list<float> durations) {
    nevergone::enemy_actions_wbg_document::Document document;
    for (const float duration : durations) {
        document.primary_records.emplace_back();
        nevergone::enemy_actions_wbg_final_table::Entry entry;
        entry.reciprocal_value = duration;
        document.final_table.entries.push_back(entry);
    }
    return document;
}

}  // namespace

int main() {
    const auto appearance = clip({0.1f, 0.2f});
    const auto state_action = clip({0.25f, 0.5f});

    auto sample = nevergone::tutorial_enemy_action_animation::sample(
            appearance, 47u, false, state_action, 28u, true, 0.05);
    assert(sample.document == &appearance);
    assert(sample.action_id == 47u && sample.frame_index == 0u);

    sample = nevergone::tutorial_enemy_action_animation::sample(
            appearance, 47u, false, state_action, 28u, true, 0.11);
    assert(sample.document == &appearance);
    assert(sample.frame_index == 1u);

    sample = nevergone::tutorial_enemy_action_animation::sample(
            appearance, 47u, false, state_action, 28u, true, 0.31);
    assert(sample.document == &state_action);
    assert(sample.action_id == 28u && sample.frame_index == 0u);

    sample = nevergone::tutorial_enemy_action_animation::sample(
            appearance, 47u, false, state_action, 28u, true, 0.56);
    assert(sample.document == &state_action && sample.frame_index == 1u);

    sample = nevergone::tutorial_enemy_action_animation::sample(
            appearance, 47u, false, state_action, 28u, true, 1.10);
    assert(sample.document == &state_action && sample.frame_index == 0u);

    // A non-looping state action is allowed to finish without reusing its last
    // frame after the clip duration has passed.
    sample = nevergone::tutorial_enemy_action_animation::sample(
            appearance, 47u, false, state_action, 28u, false, 1.06);
    assert(sample.document == nullptr);

    sample = nevergone::tutorial_enemy_action_animation::sample_action(
            state_action, 3u, false, 0.30);
    assert(sample.document == &state_action);
    assert(sample.action_id == 3u && sample.frame_index == 1u);
    sample = nevergone::tutorial_enemy_action_animation::sample_action(
            state_action, 3u, false, 0.75);
    assert(sample.document == nullptr);
    sample = nevergone::tutorial_enemy_action_animation::sample_action(
            state_action, 21u, true, 0.75);
    assert(sample.document == &state_action && sample.frame_index == 0u);

    return 0;
}
