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

// ARMv7 proves the first object begins with an int32 followed by a uint32.
// Later range construction now proves that the second value is the byte length
// of the immediately following string payload. Keep the historical field name
// here so existing probes remain source-compatible; FirstObjectCore exposes the
// newly verified string itself.
struct FirstObjectPrefix {
    FirstLayerHeader layer_header;
    std::int32_t first_i32 = 0;
    std::uint32_t second_u32 = 0;  // proven string byte length
    std::size_t bytes_consumed = 0;
};

// Strongest currently verified first-object boundary. After the prefix the
// original advances by five bytes from the string-length field start (the four
// length bytes plus one still-opaque byte), copies exactly second_u32 bytes,
// appends a NUL, then reads five floats, one int32 and two one-byte bools.
// Assignment shape proves float[0:2] and float[3:5] are CCPoint pairs, but their
// gameplay semantics are intentionally not named yet.
struct FirstObjectCore {
    FirstObjectPrefix prefix;
    std::string string_value;
    float first_point_x = 0.0f;
    float first_point_y = 0.0f;
    float middle_float = 0.0f;
    float second_point_x = 0.0f;
    float second_point_y = 0.0f;
    std::int32_t trailing_i32 = 0;
    bool first_bool = false;
    bool second_bool = false;
    std::size_t bytes_consumed = 0;
};

bool parse(const hp_data::Reader& reader, Prefix* out);
bool parse_first_scene_header(const hp_data::Reader& reader, FirstSceneHeader* out);
bool parse_first_layer_header(const hp_data::Reader& reader, FirstLayerHeader* out);
bool parse_first_object_prefix(const hp_data::Reader& reader, FirstObjectPrefix* out);

// Parse through the two proven bool fields and stop before the subsequent
// conditional object block. Output is transactional: any truncated/hostile
// string length or missing scalar leaves the caller's value unchanged.
bool parse_first_object_core(const hp_data::Reader& reader, FirstObjectCore* out);

}  // namespace nevergone::game_levels_scene_prefix
