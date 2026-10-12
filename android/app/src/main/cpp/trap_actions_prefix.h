#pragma once

#include <cstddef>
#include <cstdint>

#include "hp_data_reader.h"

namespace nevergone::trap_actions_prefix {

struct Prefix {
    std::int32_t first_i32 = 0;
    float first_float = 0.0f;
    float second_float = 0.0f;
    std::uint32_t record_count = 0;
    std::size_t bytes_consumed = 0;
};

// Bounded reconstruction of the first four reads performed by
// TrapActionsData::loadWBGFile(). The original reads these fields at exact
// offsets 0,4,8,12 before entering the record_count-controlled main loop.
// Transactional: on failure, *out is left unchanged.
bool parse(const hp_data::Reader& reader, Prefix* out);

}  // namespace nevergone::trap_actions_prefix
