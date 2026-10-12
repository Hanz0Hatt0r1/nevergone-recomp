#include <cassert>

#include "scene_actions_active_playback.h"

namespace active = nevergone::scene_actions_active_playback;
namespace docs = nevergone::scene_action_pair_document_store;
namespace combo = nevergone::trap_actions_combo_section;

int main() {
    nevergone::trap_actions_document::Document document;
    document.core.primary.prefix.record_count = 3u;
    document.core.primary.records.resize(3);
    document.final_table.primary_record_count = 3;
    document.final_table.entries = {
        {100, 10.0f, 0x5cu},
        {100, 10.0f, 0x5cu},
        {100, 10.0f, 0x5cu},
    };
    document.combo.primary_record_count = 3u;
    document.combo.tuples.resize(3u);
    document.combo.array_90_values.resize(3u);
    combo::DerivedActionComboValue a94;
    a94.target_array_offset = combo::kArray94Offset;
    a94.field_18 = 2;
    document.combo.array_94_values = {a94};
    combo::DerivedActionComboValue a98;
    a98.target_array_offset = combo::kArray98Offset;
    a98.field_18 = 2;
    document.combo.array_98_values = {a98};

    active::Runtime runtime;
    assert(runtime.start(docs::Slot::kBorn, 2, document, 50.0f, 75.0f, 7));
    const auto initial = runtime.snapshot(docs::Slot::kBorn);
    assert(initial.has_value());
    assert(initial->variant == 2);
    assert(initial->playback.frame_index_250 == 0u);
    assert(initial->parent_x == 50.0f && initial->parent_y == 75.0f);

    // Native first update clears its latch without accumulating dt.
    auto tick = runtime.tick(docs::Slot::kBorn, document, 1.0f);
    assert(tick.status == active::TickStatus::kNoAdvance);
    assert(tick.frame_request_valid && tick.frame_index == 0u);

    // 20 ms consumes two 10 ms frame thresholds. Equality with the final
    // +0x94 field_18 is still a valid displayed frame.
    tick = runtime.tick(docs::Slot::kBorn, document, 0.020f);
    assert(tick.status == active::TickStatus::kFrameChanged);
    assert(tick.frames_advanced == 2u);
    assert(tick.frame_request_valid && tick.frame_index == 2u);

    // Crossing strictly past the final +0x94 field_18 selects the recovered
    // native terminal transition. Born remains done and does not request frame 0.
    tick = runtime.tick(docs::Slot::kBorn, document, 0.010f);
    assert(tick.status == active::TickStatus::kCompleted);
    assert(!tick.frame_request_valid);
    const auto terminal = runtime.snapshot(docs::Slot::kBorn);
    assert(terminal.has_value());
    assert(terminal->playback.done_1a8);
    assert(terminal->playback.end_184);
    assert(terminal->playback.frame_index_250 == 0u);
    assert(terminal->playback.terminal_last_frame_180 == 2);

    // Later ticks remain completed without replaying the born animation.
    tick = runtime.tick(docs::Slot::kBorn, document, 1.0f);
    assert(tick.status == active::TickStatus::kCompleted);
    assert(!tick.frame_request_valid);

    runtime.stop(docs::Slot::kBorn);
    assert(!runtime.snapshot(docs::Slot::kBorn).has_value());

    // Idle mode uses the same native boundary but clears done and requests
    // updateData for frame zero immediately, closing the loop inside waUpdate.
    assert(runtime.start(docs::Slot::kIdle, 2, document, 0.0f, -45.0f, 7));
    tick = runtime.tick(docs::Slot::kIdle, document, 1.0f);
    assert(tick.status == active::TickStatus::kNoAdvance);
    tick = runtime.tick(docs::Slot::kIdle, document, 0.030f);
    assert(tick.status == active::TickStatus::kLooped);
    assert(tick.frame_request_valid && tick.frame_index == 0u);
    const auto idle = runtime.snapshot(docs::Slot::kIdle);
    assert(idle.has_value());
    assert(idle->playback.mode_24e == 1u);
    assert(!idle->playback.done_1a8);
    assert(idle->playback.end_184);
    assert(idle->playback.frame_index_250 == 0u);
    assert(idle->playback.terminal_last_frame_180 == 2);

    assert(!runtime.start(docs::Slot::kBorn, 0, document, 0.0f, 0.0f, 0));
    nevergone::trap_actions_document::Document invalid;
    assert(!runtime.start(docs::Slot::kBorn, 1, invalid, 0.0f, 0.0f, 0));
    return 0;
}
