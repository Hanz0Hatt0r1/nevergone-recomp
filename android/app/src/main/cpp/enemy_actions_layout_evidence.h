#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace nevergone::enemy_actions_layout_evidence {

// ELF32/Thumb instruction offsets recovered from the original 1.0.9 ARMv7
// library. These are metadata for clean-room probe preparation; they are not
// addresses in the reconstructed process.
constexpr std::size_t kConstructorInstructionOffset = 0x28f394u;
constexpr std::size_t kLoadWbgFileInstructionOffset = 0x28f430u;
constexpr std::size_t kInitWithFileInstructionOffset = 0x29043cu;
constexpr std::size_t kLoadWbgCallFromInitInstructionOffset = 0x29054au;

constexpr std::size_t kHpDataCreateWithContentsOfFileInstructionOffset = 0x2c652cu;
constexpr std::size_t kHpDataGetBytesCharInstructionOffset = 0x2c6568u;
constexpr std::size_t kHpDataGetBytesIntInstructionOffset = 0x2c658au;
constexpr std::size_t kHpDataGetBytesUnsignedInstructionOffset = 0x2c65acu;
constexpr std::size_t kHpDataGetBytesFloatInstructionOffset = 0x2c65ceu;
constexpr std::size_t kHpDataGetBytesBoolInstructionOffset = 0x2c65f0u;

struct InitScalarWrite {
    std::size_t offset = 0;
    std::uint32_t value_bits = 0;
};

// initWithFile writes these exact 32-bit defaults before entering loadWBGFile.
// They are kept as raw bits because semantic field names are still unresolved.
constexpr std::size_t kScalarDefaultsBeginOffset = 0x0a4u;
constexpr std::size_t kScalarDefaultsEndOffset = 0x0d4u;
extern const std::array<InitScalarWrite, 12> kInitScalarWrites;

// Every 4-byte slot from +0x14 through +0xa0 receives a newly-created CCArray
// with capacity 0x80 and is retained. This is a construction fact, not a claim
// about the semantic role of any individual array.
constexpr std::size_t kInitArrayCapacity = 0x80u;
extern const std::array<std::size_t, 36> kInitArrayPointerOffsets;

// Two observed 100-entry, 4-byte regions are initialized immediately before
// the loadWBGFile call. Raw IEEE-754 bits are recorded rather than field names.
constexpr std::size_t kObservedElementBytes = 4u;
constexpr std::size_t kObservedSampleCount = 100u;
constexpr std::size_t kFirstObservedRegionOffset = 0x0d8u;
constexpr std::size_t kSecondObservedRegionOffset = 0x268u;
constexpr std::uint32_t kFirstObservedRegionInitialBits = 0x3f800000u;  // 1.0f
constexpr std::uint32_t kSecondObservedRegionInitialBits = 0x00000000u;
constexpr std::size_t kMinimumObservedObjectBytes = 0x3f8u;

// Constructor stores zero to these ARM32 pointer-sized slots. This is an
// observed write map, not a claim that every slot has been semantically typed.
extern const std::array<std::size_t, 38> kConstructorZeroedPointerOffsets;

// Early loadWBGFile HPData reads target these object offsets in this order.
extern const std::array<std::size_t, 3> kEarlyHpDataTargetOffsets;

bool is_constructor_zeroed_pointer_offset(std::size_t offset);
bool is_early_hpdata_target_offset(std::size_t offset);
bool is_init_array_pointer_offset(std::size_t offset);
bool init_scalar_word_at(std::size_t offset, std::uint32_t* out_value_bits);

// Resolve one byte offset inside the two observed 100 x 4-byte regions.
// Returns false for indexes outside the evidence-backed range.
bool first_observed_region_element_offset(std::size_t index, std::size_t* out);
bool second_observed_region_element_offset(std::size_t index, std::size_t* out);

// Internal consistency check used by host regression tests and diagnostics.
bool observed_regions_are_consistent();

}  // namespace nevergone::enemy_actions_layout_evidence
