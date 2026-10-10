#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace nevergone::enemy_actions_wbg_topology_evidence {

// Original Never Gone 1.0.9 ARMv7 ELF32/Thumb instruction metadata. These
// offsets describe the source binary only and are not runtime addresses in the
// reconstructed application.
constexpr std::size_t kLoadWbgFileInstructionOffset = 0x28f430u;
constexpr std::size_t kNoObjectOffset = static_cast<std::size_t>(-1);

enum class ReadKind { kInt32, kUInt32, kFloat32, kBool8, kBytes };

struct HeaderRead {
    std::size_t serialized_offset = 0;
    ReadKind kind = ReadKind::kInt32;
    std::size_t destination_object_offset = kNoObjectOffset;
    std::size_t call_instruction_offset = 0;
};

extern const std::array<HeaderRead, 4> kHeaderReads;
constexpr std::size_t kHeaderBytes = 0x10u;

// The first header word is retained on the stack and later gates one fanout.
// No semantic name is assigned to it yet.
constexpr std::size_t kHeaderWord0SerializedOffset = 0x00u;
constexpr std::int32_t kVersionedFanoutGateValue = 0x68;
constexpr std::size_t kVersionedFanoutGateInstructionOffset = 0x28fc5cu;
constexpr std::size_t kLegacyVersionedGroupCount = 6u;
constexpr std::size_t kModernVersionedGroupCount = 20u;

inline std::size_t versioned_group_count(std::int32_t header_word0) {
    return header_word0 > kVersionedFanoutGateValue
            ? kModernVersionedGroupCount
            : kLegacyVersionedGroupCount;
}

// Section A: repeated variable-length ActionFrameData records. The repeat count
// is the unsigned header word written to EnemyActionsData+0xc4. Constructed
// records are appended to EnemyActionsData+0x88.
constexpr std::size_t kPrimaryCountObjectOffset = 0x0c4u;
constexpr std::size_t kPrimaryArrayObjectOffset = 0x088u;
constexpr std::size_t kPrimaryAddObjectInstructionOffset = 0x28f820u;

// Proven serialized prefix of each primary record:
//   int32 @ +0x00
//   12 x float32 @ +0x04 .. +0x30
//   bool8 @ +0x34
//   int32 @ +0x35
//   float32 @ +0x39
//   first variable segment length int32 @ +0x3d
// Three variable segments use [int32 length][one skipped byte][length bytes].
constexpr std::size_t kPrimaryFloatCount = 12u;
constexpr std::size_t kPrimaryBoolSerializedOffset = 0x34u;
constexpr std::size_t kPrimarySecondIntSerializedOffset = 0x35u;
constexpr std::size_t kPrimaryPostIntFloatSerializedOffset = 0x39u;
constexpr std::size_t kPrimaryFirstVariableLengthOffset = 0x3du;
constexpr std::size_t kPrimaryFirstVariablePayloadOffset = 0x42u;
constexpr std::size_t kVariableSegmentFramingBytes = 5u;
constexpr std::size_t kPrimaryVariableSegmentCount = 3u;
constexpr std::size_t kPrimaryRecordFixedBytesExcludingPayload = 0x4cu;

bool primary_record_bytes(
        std::size_t len0,
        std::size_t len1,
        std::size_t len2,
        std::size_t* out_bytes);

// Section B: count-prefixed fixed records appended to EnemyActionsData+0x8c.
// Each record is [int32][float32][float32].
constexpr std::size_t kSecondaryCountReadInstructionOffset = 0x28f842u;
constexpr std::size_t kSecondaryRecordBytes = 0x0cu;
constexpr std::size_t kSecondaryArrayObjectOffset = 0x08cu;
constexpr std::size_t kSecondaryAddObjectInstructionOffset = 0x28f8beu;

// Section C: file-provided outer count followed by, for each outer index, an
// unsigned inner record count. Records for index i are appended to the CCArray
// pointer at EnemyActionsData + 0x14 + 4*i. Detailed record semantics remain
// intentionally unresolved.
constexpr std::size_t kDynamicOuterCountReadInstructionOffset = 0x28f8dcu;
constexpr std::size_t kDynamicInnerCountReadInstructionOffset = 0x28f904u;
constexpr std::size_t kDynamicArrayBaseObjectOffset = 0x014u;
constexpr std::size_t kDynamicAddObjectInstructionOffset = 0x28fc3eu;

inline std::size_t dynamic_array_object_offset(std::size_t index) {
    return kDynamicArrayBaseObjectOffset + index * 4u;
}

// Section D: 6 or 20 count-prefixed groups selected by header_word0 > 0x68.
// Group i appends records to EnemyActionsData + 0x34 + 4*i.
constexpr std::size_t kVersionedGroupCountReadInstructionOffset = 0x28fc7cu;
constexpr std::size_t kVersionedArrayBaseObjectOffset = 0x034u;
constexpr std::size_t kVersionedAddObjectInstructionOffset = 0x28ffe8u;

inline std::size_t versioned_array_object_offset(std::size_t index) {
    return kVersionedArrayBaseObjectOffset + index * 4u;
}

// Section E: count-prefixed fixed records appended to EnemyActionsData+0x84.
// Each is int32 + 12 float32 + bool8, exactly 0x35 bytes.
constexpr std::size_t kRootFixedCountReadInstructionOffset = 0x290018u;
constexpr std::size_t kRootFixedRecordBytes = 0x35u;
constexpr std::size_t kRootFixedArrayObjectOffset = 0x084u;
constexpr std::size_t kRootFixedAddObjectInstructionOffset = 0x290222u;

// Section F repeats once per primary record. Each serialized tuple is four
// int32 values followed by two float32 values, exactly 0x18 bytes. The first
// float is written to this+0xd8+4*i and the second to this+0x268+4*i, linking
// the file data to the two 100-entry regions initialized by initWithFile.
constexpr std::size_t kComboTupleBytes = 0x18u;
constexpr std::size_t kComboFirstFloatSerializedOffset = 0x10u;
constexpr std::size_t kComboSecondFloatSerializedOffset = 0x14u;
constexpr std::size_t kComboFirstFloatDestinationBase = 0x0d8u;
constexpr std::size_t kComboSecondFloatDestinationBase = 0x268u;
extern const std::array<std::size_t, 6> kComboTupleReadInstructionOffsets;
extern const std::array<std::size_t, 3> kComboArrayObjectOffsets;
extern const std::array<std::size_t, 3> kComboAddObjectInstructionOffsets;

inline std::size_t combo_first_float_destination(std::size_t index) {
    return kComboFirstFloatDestinationBase + index * 4u;
}

inline std::size_t combo_second_float_destination(std::size_t index) {
    return kComboSecondFloatDestinationBase + index * 4u;
}

// Section G is a final table of one int32 per primary record. For each entry,
// when the primary array is nonempty, the original code stores
// 1.0f / float(value) into ActionFrameData+0x5c for objectAtIndex(i).
constexpr std::size_t kFinalTableEntryBytes = 4u;
constexpr std::size_t kFinalTableReadInstructionOffset = 0x2903f2u;
constexpr std::size_t kFinalPrimaryArrayObjectOffset = 0x088u;
constexpr std::size_t kFinalActionFrameDestinationOffset = 0x05cu;
constexpr std::size_t kFinalStoreInstructionOffset = 0x29041au;

bool topology_is_consistent();

}  // namespace nevergone::enemy_actions_wbg_topology_evidence
