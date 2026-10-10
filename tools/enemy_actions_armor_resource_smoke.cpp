#include <cassert>

#include "enemy_actions_armor_resource.h"

int main() {
    using namespace nevergone;

    enemy_actions_wbg_prefix::NestedActionFrameRecord record;
    record.first_string = "armor.plist";

    auto r = enemy_actions_armor_resource::apply(record, 0, 0, 5, 3, 9);
    assert(r.formatted_path_available);
    assert(r.formatted_path == "enemy03/res/armor.plist");
    assert(r.native_would_reach_formatted_load_block);
    assert(r.should_add_sprite_frames);
    assert(r.should_record_enemy_object_res);

    r = enemy_actions_armor_resource::apply(record, 0, 1, 0x16, 4, 9);
    assert(r.formatted_path == "npc04/res/armor.plist");
    assert(!r.native_would_reach_formatted_load_block);
    assert(!r.should_add_sprite_frames);

    r = enemy_actions_armor_resource::apply(record, 0, 2, 0, 5, 9);
    assert(r.formatted_path == "pet05/res/armor.plist");
    assert(r.should_add_sprite_frames);

    r = enemy_actions_armor_resource::apply(record, 0, 3, 0, 6, 7);
    assert(r.formatted_path == "l06/w_res/L06_W07_default.plist");
    assert(r.should_add_sprite_frames);
    assert(r.should_record_enemy_object_res);

    r = enemy_actions_armor_resource::apply(record, 1, 3, 0, 8, 7);
    assert(r.formatted_path == "l08/res/armor.plist");
    assert(r.should_add_sprite_frames);

    r = enemy_actions_armor_resource::apply(record, 2, 3, 0, 8, 7);
    assert(!r.formatted_path_available);
    assert(r.unresolved_formatted_path_register);
    assert(r.native_would_reach_formatted_load_block);
    assert(!r.should_add_sprite_frames);

    record.first_string = "T_E01_default.plist";
    r = enemy_actions_armor_resource::apply(record, 7, 99, 0, 1, 2);
    assert(r.special_direct_plist);
    assert(r.should_add_sprite_frames);
    assert(r.sprite_frames_file == "T_E01_default.plist");
    assert(!r.should_record_enemy_object_res);

    record.first_string = "armor.plist";
    r = enemy_actions_armor_resource::apply(record, 0, 0, 0x22, 3, 9);
    assert(!r.native_would_reach_formatted_load_block);

    r = enemy_actions_armor_resource::apply(record, 0, 1, 0x18, 3, 9);
    assert(r.native_would_reach_formatted_load_block);
    assert(r.should_add_sprite_frames);

    return 0;
}
