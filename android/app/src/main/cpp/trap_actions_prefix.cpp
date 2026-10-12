#include "trap_actions_prefix.h"

#include <cmath>

namespace nevergone::trap_actions_prefix {

bool parse(const hp_data::Reader& reader, Prefix* out) {
    if (out == nullptr) return false;

    hp_data::Cursor cursor(reader);
    Prefix parsed;
    if (!cursor.read_i32_le(&parsed.first_i32) ||
        !cursor.read_f32_le(&parsed.first_float) ||
        !cursor.read_f32_le(&parsed.second_float) ||
        !cursor.read_u32_le(&parsed.record_count)) {
        return false;
    }

    // HPData::getBytes reads raw IEEE-754 values. Reject non-finite values at
    // the project boundary so malformed imported files cannot seed invalid
    // transforms/timing into the later SceneActions runtime.
    if (!std::isfinite(parsed.first_float) || !std::isfinite(parsed.second_float)) {
        return false;
    }

    parsed.bytes_consumed = cursor.offset();
    if (parsed.bytes_consumed != 16u) return false;
    *out = parsed;
    return true;
}

}  // namespace nevergone::trap_actions_prefix
