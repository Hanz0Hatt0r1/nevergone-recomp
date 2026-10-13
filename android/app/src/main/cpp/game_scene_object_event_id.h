#pragma once

#include <cstdint>
#include <cstring>

#include "game_levels_scene_prefix.h"

namespace nevergone::game_scene_object_event_id {

// LoadGL_Scene stores the first uint32 from the nonzero-object conditional
// header at GameSceneLayerObjectData+0x48. GameSceneLayerObjectData::init()
// initializes +0x48 to zero for objects that skip that header, and
// GameSceneObject::initWithData() copies the same 32 bits from +0x48 to
// GameSceneObject+0x108. showEventObjTip() later compares that retained field
// against the requested event ID.
inline std::int32_t from_record(
        const game_levels_scene_prefix::ObjectRecord& record) {
    if (!record.conditional_present) return 0;

    // Preserve the native 32-bit payload exactly rather than relying on an
    // implementation-defined unsigned-to-signed narrowing conversion.
    static_assert(sizeof(std::int32_t) == sizeof(std::uint32_t));
    std::int32_t value = 0;
    const std::uint32_t bits = record.conditional_first_u32;
    std::memcpy(&value, &bits, sizeof(value));
    return value;
}

}  // namespace nevergone::game_scene_object_event_id
