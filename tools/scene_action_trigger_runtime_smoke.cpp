#include <array>
#include <cassert>

#include "scene_action_trigger_runtime.h"

namespace trigger_plan = nevergone::scene_action_trigger_plan;
namespace trigger_runtime = nevergone::scene_action_trigger_runtime;

int main() {
    trigger_plan::Plan plan;
    plan.source_layer_index = 5u;
    plan.source_object_index = 2u;
    plan.slots[0] = trigger_plan::SlotPlan{0u, 0u, 100.0f, 235.0f, 7};
    plan.slots[1] = trigger_plan::SlotPlan{1u, 1u, 100.0f, 55.0f, 7};

    trigger_runtime::Runtime runtime;
    std::array<trigger_runtime::Command, trigger_plan::kSlotCount> commands{};
    assert(runtime.trigger(plan, &commands) == trigger_runtime::TriggerResult::kStarted);
    assert(commands[0].slot_index == 0u);
    assert(commands[0].mode_24e == 0u);
    assert(commands[0].local_x == 100.0f);
    assert(commands[0].local_y == 235.0f);
    assert(commands[0].child_z_order == 7);
    assert(commands[0].call_play);
    assert(commands[0].attach_child);
    assert(commands[1].slot_index == 1u);
    assert(commands[1].mode_24e == 1u);
    assert(commands[1].local_y == 55.0f);
    assert(commands[1].call_play);
    assert(commands[1].attach_child);

    const auto first_snapshot = runtime.snapshot();
    assert(first_snapshot.trigger_consumed);
    assert(first_snapshot.trigger_count == 1u);

    // GameSceneObject+0x105 makes triggerSceneAction one-shot.
    auto unchanged = commands;
    assert(runtime.trigger(plan, &unchanged) ==
           trigger_runtime::TriggerResult::kAlreadyConsumed);
    assert(runtime.snapshot().trigger_count == 1u);

    runtime.reset();
    assert(!runtime.snapshot().trigger_consumed);
    assert(runtime.snapshot().trigger_count == 0u);

    auto invalid = plan;
    invalid.slots[1].mode_24e = 0u;
    assert(runtime.trigger(invalid, &commands) ==
           trigger_runtime::TriggerResult::kInvalidPlan);
    assert(!runtime.snapshot().trigger_consumed);

    assert(runtime.trigger(plan, nullptr) == trigger_runtime::TriggerResult::kInvalidPlan);
    assert(!runtime.snapshot().trigger_consumed);

    return 0;
}
