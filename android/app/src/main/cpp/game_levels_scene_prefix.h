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

// At the beginning of each layer, ordered call/control-flow evidence shows one
// float followed by a uint32 used directly as the layer's object-loop bound.
// The float's semantic field name is not yet recovered.
struct FirstLayerHeader {
    FirstSceneHeader scene_header;
    float first_float = 0.0f;
    std::uint32_t object_count = 0;
    std::size_t bytes_consumed = 0;
};

// The beginning of each GameSceneLayerObjectData record is proven to contain
// an int32 followed by a uint32. Focused ARMv7 range construction now proves
// that the uint32 is the byte length of the immediately following string field.
struct FirstObjectPrefix {
    FirstLayerHeader layer_header;
    std::int32_t first_i32 = 0;
    std::uint32_t string_length = 0;
    std::size_t bytes_consumed = 0;
};

// After the object string length, the original advances by five bytes from the
// start of that uint32: four length bytes plus one skipped byte. It then reads
// exactly string_length chars, five floats, one int32 and two one-byte bools.
// The first two and next two floats are assigned through CCPoint::operator=;
// semantic meanings beyond that structural fact remain intentionally opaque.
// Parsing stops before the following version-dependent object fields.
struct FirstObjectHeader {
    FirstObjectPrefix prefix;
    std::string first_string;
    float first_point_x = 0.0f;
    float first_point_y = 0.0f;
    float first_float = 0.0f;
    float second_point_x = 0.0f;
    float second_point_y = 0.0f;
    std::int32_t second_i32 = 0;
    bool first_bool = false;
    bool second_bool = false;
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

// Parse the verified beginning of the first layer in the first scene. This
// fails when the first scene has no layers. The output is updated only after
// both the opaque float and proven object_count are available.
bool parse_first_layer_header(const hp_data::Reader& reader, FirstLayerHeader* out);

// Parse the verified 8-byte prefix of the first object. This fails when
// object_count is zero. string_length is proven by its direct use as the
// HPRange byte length and as the NUL-terminator index after the char copy.
bool parse_first_object_prefix(const hp_data::Reader& reader, FirstObjectPrefix* out);

// Parse the strongest currently verified non-versioned first-object boundary.
// The one-byte gap before the string remains semantically unnamed. Output is
// transactional and no version-dependent bytes after the two bools are read.
bool parse_first_object_header(const hp_data::Reader& reader, FirstObjectHeader* out);

}  // namespace nevergone::game_levels_scene_prefix
