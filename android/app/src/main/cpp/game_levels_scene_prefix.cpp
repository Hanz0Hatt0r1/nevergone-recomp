#include "game_levels_scene_prefix.h"

namespace nevergone::game_levels_scene_prefix {

bool parse(const hp_data::Reader& reader, Prefix* out) {
    if (out == nullptr) return false;

    hp_data::Cursor cursor(reader);
    Prefix parsed;
    if (!cursor.read_i32_le(&parsed.first_i32)) return false;
    if (!cursor.read_u32_le(&parsed.second_u32)) return false;
    if (!cursor.read_u32_le(&parsed.third_u32)) return false;
    parsed.bytes_consumed = cursor.offset();

    *out = parsed;
    return true;
}

}  // namespace nevergone::game_levels_scene_prefix
