#include <cassert>
#include <cmath>
#include <cstdint>
#include <cstring>

#include "enemy_actions_armor_sprite_update.h"

namespace {
std::uint32_t bits(float value) {
    std::uint32_t out = 0;
    std::memcpy(&out, &value, sizeof(out));
    return out;
}
}

int main() {
    using namespace nevergone;

    enemy_actions_wbg_prefix::NestedActionFrameRecord record;
    record.third_string = "armor_frame.png";
    record.float_values[0] = 10.0f;
    record.float_values[1] = 20.0f;
    record.float_values[2] = 0.25f;
    record.float_values[3] = 0.75f;
    record.float_values[6] = 100.0f;
    record.float_values[7] = 200.0f;
    record.float_values[9] = 30.0f;
    record.float_values[10] = 1.5f;
    record.float_values[11] = 0.5f;
    record.second_i32 = 0x1234;
    record.first_bool = false;

    auto r = enemy_actions_armor_sprite_update::apply(
            record, 2, 3.0f, 4.0f, 5.0f, 0, 0, 0);
    assert(r.frame_name_78 == "armor_frame.png");
    assert(r.base_position.x == 13.0f);
    assert(r.base_position.y == 19.0f);
    assert(r.final_position.x == 13.0f);
    assert(r.final_position.y == 19.0f);
    assert(r.anchor.x == 0.25f && r.anchor.y == 0.75f);
    assert(r.scale_x == 1.5f && r.scale_y == 0.5f);
    assert(r.rotation == 30.0f);
    assert(r.visibility_from_field_258);
    assert(!r.flip_x);
    assert(!r.applied_flip_geometry);
    assert(r.opacity_50 == 0x34u);
    assert(r.final_visible);

    record.first_bool = true;
    r = enemy_actions_armor_sprite_update::apply(
            record, 2, 3.0f, 4.0f, 5.0f, 1, 0, 0);
    assert(r.flip_x);
    assert(r.applied_flip_geometry);
    assert(r.final_position.x == 187.0f);
    assert(r.final_position.y == 19.0f);
    assert(r.anchor.x == 0.75f);
    assert(bits(r.rotation) == (bits(30.0f) ^ 0x80000000u));
    assert(!r.final_visible);

    r = enemy_actions_armor_sprite_update::apply(
            record, 2, 3.0f, 4.0f, 5.0f, 1, 1, 3);
    assert(!r.flip_x);
    assert(!r.applied_flip_geometry);
    assert(r.mode3_forces_visible);
    assert(r.final_visible);

    record.first_bool = true;
    record.float_values[9] = 0.0f;
    r = enemy_actions_armor_sprite_update::apply(
            record, 0, 0.0f, 0.0f, 0.0f, 0, 0, 0);
    assert(bits(r.rotation) == 0x80000000u);

    return 0;
}
