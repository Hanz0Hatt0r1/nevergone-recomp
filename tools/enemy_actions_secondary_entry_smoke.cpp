#include <cassert>

#include "enemy_actions_secondary_entry.h"

int main() {
    using nevergone::enemy_actions_secondary_entry::apply;
    using nevergone::enemy_actions_wbg_prefix::CompactActionFrameBlock;
    using nevergone::enemy_actions_wbg_prefix::CompactActionFrameRecord;

    CompactActionFrameBlock block;
    block.records.push_back(CompactActionFrameRecord{10, 1.0f, 2.0f});
    block.records.push_back(CompactActionFrameRecord{11, 3.0f, 4.0f});

    const auto selected = apply(1, true, true, block);
    assert(selected.enemy_actions_data_present);
    assert(selected.secondary_array_present);
    assert(selected.secondary_count == 2u);
    assert(selected.native_would_object_at_index);
    assert(selected.requested_index == 1u);
    assert(selected.selected_record_available);
    assert(selected.selected_field_18 == 4.0f);

    const auto no_ead = apply(0, false, true, block);
    assert(!no_ead.native_would_object_at_index);

    const auto no_array = apply(0, true, false, block);
    assert(!no_array.native_would_object_at_index);

    CompactActionFrameBlock empty;
    const auto no_records = apply(0, true, true, empty);
    assert(!no_records.native_would_object_at_index);

    // Native would still call objectAtIndex when count is nonzero; project
    // storage reports that intent but refuses to dereference outside its vector.
    const auto out_of_range = apply(2, true, true, block);
    assert(out_of_range.native_would_object_at_index);
    assert(out_of_range.requested_index == 2u);
    assert(!out_of_range.selected_record_available);

    // Negative int32 frame bits become a large uint32 index in the native call.
    const auto negative = apply(-1, true, true, block);
    assert(negative.native_would_object_at_index);
    assert(negative.requested_index == 0xffffffffu);
    assert(!negative.selected_record_available);

    return 0;
}
