#include <array>
#include <cassert>

#include "enemy_actions_armor_entry.h"

int main() {
    using namespace nevergone;

    enemy_actions_wbg_document::Document document;
    document.nested_block.groups.resize(8);

    enemy_actions_wbg_prefix::NestedActionFrameRecord miss;
    miss.first_i32 = 11;
    enemy_actions_wbg_prefix::NestedActionFrameRecord match0;
    match0.first_i32 = 12;
    enemy_actions_wbg_prefix::NestedActionFrameRecord match1;
    match1.first_i32 = 12;

    document.nested_block.groups[0].records = {miss, match0, match1};
    document.nested_block.groups[3].records = {miss};

    const std::array<bool, enemy_actions_armor_entry::kArmorSlotCount> sprites = {
            true, false, true, false, true, false, true, false};

    const auto result = enemy_actions_armor_entry::apply(document, 12, sprites);

    assert(result.slots[0].sprite_object_offset == 0x130u);
    assert(result.slots[7].sprite_object_offset == 0x14cu);
    assert(result.slots[0].array_object_offset == 0x14u);
    assert(result.slots[7].array_object_offset == 0x30u);

    assert(result.slots[0].should_set_visible_false);
    assert(!result.slots[1].should_set_visible_false);
    assert(result.slots[6].should_set_visible_false);

    assert(result.slots[0].serialized_group_available);
    assert(result.slots[0].serialized_record_count == 3u);
    assert(result.slots[0].native_would_scan_records);
    assert(result.slots[0].matching_record_available);
    assert(result.slots[0].selected_record_index == 1u);
    assert(result.slots[0].selected_field_74 == 12);

    assert(result.slots[3].native_would_scan_records);
    assert(!result.slots[3].matching_record_available);
    assert(!result.slots[4].native_would_scan_records);

    enemy_actions_wbg_document::Document short_document;
    short_document.nested_block.groups.resize(2);
    const auto short_result = enemy_actions_armor_entry::apply(short_document, 0, sprites);
    assert(short_result.slots[1].serialized_group_available);
    assert(!short_result.slots[2].serialized_group_available);
    assert(short_result.slots[2].should_set_visible_false);

    return 0;
}
