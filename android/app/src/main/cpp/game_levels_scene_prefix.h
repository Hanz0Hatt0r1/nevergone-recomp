#pragma once

#include <cstddef>
#include <cstdint>

#include "hp_data_reader.h"

namespace nevergone::game_levels_scene_prefix {

// The broad checked-in call metadata establishes only the primitive order at
// the beginning of GameLevels::LoadGL_Scene: signed int32 followed by uint32.
// Their semantic field names are not yet recovered, so keep them deliberately
// opaque until focused reverse-engineering evidence identifies them.
struct Prefix {
    std::int32_t first_i32 = 0;
    std::uint32_t second_u32 = 0;
    std::size_t bytes_consumed = 0;
};

// Parse only the verified two-field LoadGL_Scene prefix. The output is updated
// atomically on success; truncated input leaves it unchanged.
bool parse(const hp_data::Reader& reader, Prefix* out);

}  // namespace nevergone::game_levels_scene_prefix
