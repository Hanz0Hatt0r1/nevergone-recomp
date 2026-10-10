#include <cassert>
#include <cmath>
#include <limits>

#include "enemy_actions_armor_z_order.h"

int main() {
    using namespace nevergone;

    enemy_actions_wbg_prefix::NestedActionFrameRecord record;

    record.float_values[8] = 3.75f;
    auto r = enemy_actions_armor_z_order::apply(record, 2);
    assert(r.should_reorder_child);
    assert(r.conversion_resolved);
    assert(r.field_38 == 3);
    assert(r.z_order == 3);

    record.float_values[8] = -3.75f;
    r = enemy_actions_armor_z_order::apply(record, 7);
    assert(r.conversion_resolved);
    assert(r.field_38 == -3);

    record.float_values[8] = -2147483648.0f;
    r = enemy_actions_armor_z_order::apply(record, 0);
    assert(r.conversion_resolved);
    assert(r.field_38 == std::numeric_limits<std::int32_t>::min());

    record.float_values[8] = 2147483648.0f;
    r = enemy_actions_armor_z_order::apply(record, 0);
    assert(!r.conversion_resolved);
    assert(r.should_reorder_child);

    record.float_values[8] = std::numeric_limits<float>::infinity();
    r = enemy_actions_armor_z_order::apply(record, 0);
    assert(!r.conversion_resolved);

    record.float_values[8] = std::numeric_limits<float>::quiet_NaN();
    r = enemy_actions_armor_z_order::apply(record, 0);
    assert(!r.conversion_resolved);

    return 0;
}
