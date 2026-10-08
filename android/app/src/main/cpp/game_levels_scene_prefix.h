#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

#include "hp_data_reader.h"

namespace nevergone::game_levels_scene_prefix {

struct Prefix {
    std::int32_t first_i32 = 0;
    std::uint32_t scene_count = 0;
    std::size_t bytes_consumed = 0;
};

struct FirstSceneHeader {
    Prefix prefix;
    std::uint32_t first_string_length = 0;
    std::string first_string;
    float first_point_x = 0.0f;
    float first_point_y = 0.0f;
    std::uint32_t layer_count = 0;
    std::size_t bytes_consumed = 0;
};

struct FirstLayerHeader {
    FirstSceneHeader scene_header;
    float first_float = 0.0f;
    std::uint32_t object_count = 0;
    std::size_t bytes_consumed = 0;
};

// The beginning of each GameSceneLayerObjectData record is proven to contain
// an int32 followed immediately by a uint32 before the first unresolved-width
// char field. Their semantic identities are still unknown.
struct FirstObjectPrefix {
    FirstLayerHeader layer_header;
    std::int32_t first_i32 = 0;
    std::uint32_t second_u32 = 0;
    std::size_t bytes_consumed = 0;
};

bool parse(const hp_data::Reader& reader, Prefix* out);
bool parse_first_scene_header(const hp_data::Reader& reader, FirstSceneHeader* out);
bool parse_first_layer_header(const hp_data::Reader& reader, FirstLayerHeader* out);

// Parse only the verified 8-byte prefix of the first object. This fails when
// object_count is zero and deliberately stops before the following char field.
bool parse_first_object_prefix(const hp_data::Reader& reader, FirstObjectPrefix* out);

}  // namespace nevergone::game_levels_scene_prefix
