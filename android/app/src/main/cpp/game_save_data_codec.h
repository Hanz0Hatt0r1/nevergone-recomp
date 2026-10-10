#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

namespace nevergone::game_save_data_codec {

// Reconstructs the leaf transforms exported as GameSaveData::Encode/Decode in
// the shipped ARMv7 library. The native routines ignore `this`, return length,
// and advance an 8-bit byte transform counter through 0..126 before wrapping.
//
// For length > 0, source and destination must both be non-null. The original
// routines' malformed-pointer behavior was not probed; this project-owned API
// returns 0 for invalid pointers rather than claiming native-equivalent UB.
std::size_t encode(
    const std::uint8_t* source,
    std::size_t length,
    std::uint8_t* destination,
    std::uint32_t key);

std::size_t decode(
    const std::uint8_t* source,
    std::size_t length,
    std::uint8_t* destination,
    std::uint32_t key);

std::vector<std::uint8_t> encode_copy(
    const std::vector<std::uint8_t>& source,
    std::uint32_t key);

std::vector<std::uint8_t> decode_copy(
    const std::vector<std::uint8_t>& source,
    std::uint32_t key);

}  // namespace nevergone::game_save_data_codec
