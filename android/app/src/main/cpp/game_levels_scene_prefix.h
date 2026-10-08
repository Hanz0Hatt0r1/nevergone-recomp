#pragma once

#include <cstddef>
#include <cstdint>

#include "hp_data_reader.h"

namespace nevergone::game_levels_scene_prefix {

// The checked-in call metadata establishes the primitive order at the start of
// GameLevels::LoadGL_Scene: int32, uint32, GameSceneData::create, then uint32.
// Object creation does not consume HPData bytes, so the first three fields are
// sequentially readable. Their semantic identities are not yet recovered.
struct Prefix {
    std::int32_t first_i32 = 0;
    std::uint32_t second_u32 = 0;
    std::uint32_t third_u32 = 0;
    std::size_t bytes_consumed = 0;
};

// Parse only the verified three-field LoadGL_Scene prefix. The output is
// updated atomically on success; truncated input leaves it unchanged.
bool parse(const hp_data::Reader& reader, Prefix* out);

}  // namespace nevergone::game_levels_scene_prefix
