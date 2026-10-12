#include <cassert>

#include "scene_actions_runtime_controller.h"

namespace controller = nevergone::scene_actions_runtime_controller;
namespace docs = nevergone::scene_action_pair_document_store;
namespace trigger = nevergone::scene_action_trigger_plan;
namespace combo = nevergone::trap_actions_combo_section;

static nevergone::trap_actions_document::Document document() {
    nevergone::trap_actions_document::Document out;
    out.core.primary.prefix.record_count = 2u;
    out.core.primary.records.resize(2);
    out.final_table.primary_record_count = 2;
    out.final_table.entries = {
        {100, 10.0f, 0x5cu},
        {100, 10.0f, 0x5cu},
    };
    out.combo.primary_record_count = 2u;
    out.combo.tuples.resize(2u);
    out.combo.array_90_values.resize(2u);
    combo::DerivedActionComboValue a94;
    a94.target_array_offset = combo::kArray94Offset;
    a94.field_18 = 1;
    out.combo.array_94_values = {a94};
    combo::DerivedActionComboValue a98;
    a98.target_array_offset = combo::kArray98Offset;
    a98.field_18 = 1;
    out.combo.array_98_values = {a98};
    return out;
}

int main() {
    trigger::Plan plan;
    plan.slots[0] = trigger::SlotPlan{0u, 0u, 12.0f, 135.0f, 3};
    plan.slots[1] = trigger::SlotPlan{1u, 1u, 12.0f, -45.0f, 3};

    const auto born = document();
    const auto idle = document();

    controller::Controller runtime;
    auto started = runtime.start_with_documents(plan, 3, born, idle);
    assert(started.status == controller::StartStatus::kStarted);
    assert(runtime.started() && runtime.variant() == 3);

    const auto born_state = runtime.snapshot(docs::Slot::kBorn);
    const auto idle_state = runtime.snapshot(docs::Slot::kIdle);
    assert(born_state.has_value() && idle_state.has_value());
    assert(born_state->parent_x == 12.0f && born_state->parent_y == 135.0f);
    assert(idle_state->parent_y == -45.0f);
    assert(born_state->child_z_order == 3 && idle_state->child_z_order == 3);

    // First update preserves the recovered first-update latch semantics.
    auto tick = runtime.tick_with_document(docs::Slot::kBorn, born, 1.0f);
    assert(tick.status == nevergone::scene_actions_active_playback::TickStatus::kNoAdvance);
    assert(tick.frame_request_valid && tick.frame_index == 0u);

    tick = runtime.tick_with_document(docs::Slot::kBorn, born, 0.010f);
    assert(tick.status == nevergone::scene_actions_active_playback::TickStatus::kFrameChanged);
    assert(tick.frame_request_valid && tick.frame_index == 1u);

    // One-shot trigger gate remains consumed even if the caller retries.
    started = runtime.start_with_documents(plan, 3, born, idle);
    assert(started.status == controller::StartStatus::kAlreadyConsumed);

    runtime.reset();
    assert(!runtime.started());
    started = runtime.start_with_documents(plan, 0, born, idle);
    assert(started.status == controller::StartStatus::kPairUnavailable);

    trigger::Plan invalid = plan;
    invalid.slots[1].mode_24e = 0u;
    started = runtime.start_with_documents(invalid, 1, born, idle);
    assert(started.status == controller::StartStatus::kInvalidPlan);
    assert(!runtime.started());

    nevergone::trap_actions_document::Document empty;
    started = runtime.start_with_documents(plan, 1, empty, idle);
    assert(started.status == controller::StartStatus::kPairUnavailable);
    return 0;
}
