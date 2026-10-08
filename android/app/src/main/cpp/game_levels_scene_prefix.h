#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

#include "hp_data_reader.h"

namespace nevergone::game_levels_scene_prefix {

// The original LoadGL_Scene stream begins with one unresolved signed int32
// stored on GameLevels, followed by a uint32 used directly as the scene-loop
// bound. No scene-local bytes are consumed when scene_count is zero.
struct Prefix {
    std::int32_t first_i32 = 0;
    std::uint32_t scene_count = 0;
    std::size_t bytes_consumed = 0;
};

// Within each scene, focused ARMv7 evidence shows a uint32 byte length, one
// skipped byte of still-unknown meaning, a char payload of exactly that length,
// two floats assigned as a CCPoint, then a uint32 used as the layer-loop bound.
struct FirstSceneHeader {
    Prefix prefix;
    std::uint32_t first_string_length = 0;
    std::string first_string;
    float first_point_x = 0.0f;
    float first_point_y = 0.0f;
    std::uint32_t layer_count = 0;
    std::size_t bytes_consumed = 0;
};

// Parse only the verified top-level LoadGL_Scene prefix. The output is updated
// atomically on success; truncated input leaves it unchanged.
bool parse(const hp_data::Reader& reader, Prefix* out);

// Parse the verified beginning of the first scene. This fails when scene_count
// is zero because no first scene exists. The skipped byte is deliberately not
// exposed as a semantic field. Output is updated only after the complete
// verified header, including layer_count, is present.
bool parse_first_scene_header(const hp_data::Reader& reader, FirstSceneHeader* out);

}  // namespace nevergone::game_levels_scene_prefix
