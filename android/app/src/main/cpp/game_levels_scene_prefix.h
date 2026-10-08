#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

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

// Focused ARMv7 disassembly establishes that the third uint32 is then used as
// the byte length for the first char payload. The original parser skips one
// still-unidentified byte, copies exactly that many bytes, appends a NUL, and
// reads two 32-bit floats that are assigned as a CCPoint.
struct FirstRecordHeader {
    Prefix prefix;
    std::string first_string;
    float first_point_x = 0.0f;
    float first_point_y = 0.0f;
    std::size_t bytes_consumed = 0;
};

// Parse only the verified three-field LoadGL_Scene prefix. The output is
// updated atomically on success; truncated input leaves it unchanged.
bool parse(const hp_data::Reader& reader, Prefix* out);

// Extend the verified prefix through the first string and point. The one byte
// between the string length and payload is skipped but deliberately not given
// a semantic meaning. Output is updated only after the whole verified header
// is present.
bool parse_first_record_header(const hp_data::Reader& reader, FirstRecordHeader* out);

}  // namespace nevergone::game_levels_scene_prefix
