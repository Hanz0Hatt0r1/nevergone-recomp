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
    const auto idle = clip({0.25f, 0.5f});

    auto sample = nevergone::tutorial_enemy_action_animation::sample(
            appearance, 47u, idle, 21u, 0.05);
    assert(sample.document == &appearance);
    assert(sample.action_id == 47u && sample.frame_index == 0u);

    sample = nevergone::tutorial_enemy_action_animation::sample(
            appearance, 47u, idle, 21u, 0.11);
    assert(sample.document == &appearance);
    assert(sample.frame_index == 1u);

    sample = nevergone::tutorial_enemy_action_animation::sample(
            appearance, 47u, idle, 21u, 0.31);
    assert(sample.document == &idle);
    assert(sample.action_id == 21u && sample.frame_index == 0u);

    sample = nevergone::tutorial_enemy_action_animation::sample(
            appearance, 47u, idle, 21u, 0.56);
    assert(sample.document == &idle && sample.frame_index == 1u);

    sample = nevergone::tutorial_enemy_action_animation::sample(
            appearance, 47u, idle, 21u, 1.10);
    assert(sample.document == &idle && sample.frame_index == 0u);

    return 0;
}
