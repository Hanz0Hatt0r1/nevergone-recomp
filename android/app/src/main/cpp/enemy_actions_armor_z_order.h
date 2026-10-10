#pragma once

#include <cmath>
#include <cstddef>
#include <cstdint>

#include "enemy_actions_wbg_prefix.h"

namespace nevergone::enemy_actions_armor_z_order {

struct Result {
    std::size_t armor_id = 0;
    float serialized_float_8 = 0.0f;
    bool conversion_resolved = false;
    std::int32_t field_38 = 0;
    bool should_reorder_child = true;
    std::int32_t z_order = 0;
};

inline Result apply(
        const enemy_actions_wbg_prefix::NestedActionFrameRecord& record,
        std::size_t armor_id) {
    Result result;
    result.armor_id = armor_id;
    result.serialized_float_8 = record.float_values[8];

    const double value = static_cast<double>(record.float_values[8]);
    if (!std::isfinite(value) ||
        value < -2147483648.0 ||
        value >= 2147483648.0) {
        return result;
    }

    // VCVT.S32.F32 without an explicit rounding-mode suffix uses round toward
    // zero for this conversion form. Keep exceptional/out-of-range cases above
    // unresolved rather than inventing an architectural exception result.
    result.field_38 = static_cast<std::int32_t>(value);
    result.z_order = result.field_38;
    result.conversion_resolved = true;
    return result;
}

}  // namespace nevergone::enemy_actions_armor_z_order
