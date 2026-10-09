#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "hp_data_reader.h"

namespace nevergone::game_levels_scene_prefix {

struct Prefix {
    // The original stores this value directly at GameLevels+0x3c and uses it
    // as serialized-format gates (>1 and >2). Keep the historical field name
    // for source compatibility while documenting its now-proven role.
    std::int32_t first_i32 = 0;  // format/version gate
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

struct FirstObjectPrefix {
    FirstLayerHeader layer_header;
    std::int32_t first_i32 = 0;
    std::uint32_t second_u32 = 0;  // proven string byte length
    std::size_t bytes_consumed = 0;
};

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

// For serialized format versions >2, ARMv7 reads a uint32 count immediately
// after FirstObjectCore and then exactly count uint32 values, appending each to
// a vector owned by the object. Versions <=2 have no bytes for this block.
// The values' gameplay meaning is not yet proven, so the clean-room model uses
// neutral structural names only.
struct FirstObjectVersionedList {
    FirstObjectCore core;
    bool present = false;
    std::uint32_t value_count = 0;
    std::vector<std::uint32_t> values;
    std::size_t bytes_consumed = 0;
};

bool parse(const hp_data::Reader& reader, Prefix* out);
bool parse_first_scene_header(const hp_data::Reader& reader, FirstSceneHeader* out);
bool parse_first_layer_header(const hp_data::Reader& reader, FirstLayerHeader* out);
bool parse_first_object_prefix(const hp_data::Reader& reader, FirstObjectPrefix* out);
bool parse_first_object_core(const hp_data::Reader& reader, FirstObjectCore* out);

// Advance through the version>2 uint32 vector when present, or return the core
// boundary unchanged for older versions. Count is bounds-checked against the
// remaining stream before allocating/reading any values.
bool parse_first_object_versioned_list(
    const hp_data::Reader& reader,
    FirstObjectVersionedList* out);

}  // namespace nevergone::game_levels_scene_prefix
