#include "enemy_actions_wbg_topology_evidence.h"

#include <limits>

namespace nevergone::enemy_actions_wbg_topology_evidence {

const std::array<HeaderRead, 4> kHeaderReads{{
    {0x00u, ReadKind::kInt32, kNoObjectOffset, 0x28f4a0u},
    {0x04u, ReadKind::kFloat32, 0x0d4u, 0x28f4b4u},
    {0x08u, ReadKind::kFloat32, 0x0d0u, 0x28f4cau},
    {0x0cu, ReadKind::kUInt32, 0x0c4u, 0x28f4e2u},
}};

const std::array<std::size_t, 6> kComboTupleReadInstructionOffsets{{
    0x29027au, 0x29028eu, 0x2902acu, 0x2902ceu, 0x2902f8u, 0x290328u,
}};

const std::array<std::size_t, 3> kComboArrayObjectOffsets{{0x094u, 0x09cu, 0x098u}};
const std::array<std::size_t, 3> kComboAddObjectInstructionOffsets{{
    0x290368u, 0x290384u, 0x2903b4u,
}};

bool primary_record_bytes(
        std::size_t len0,
        std::size_t len1,
        std::size_t len2,
        std::size_t* out_bytes) {
    if (out_bytes == nullptr) return false;
    constexpr std::size_t kMax = std::numeric_limits<std::size_t>::max();
    const std::array<std::size_t, 3> lengths{{len0, len1, len2}};
    std::size_t total = kPrimaryRecordFixedBytesExcludingPayload;
    for (const std::size_t length : lengths) {
        if (length > kMax - total) return false;
        total += length;
    }
    *out_bytes = total;
    return true;
}

bool topology_is_consistent() {
    if (kHeaderReads.front().serialized_offset != 0u ||
            kHeaderReads.back().serialized_offset + 4u != kHeaderBytes) return false;
    for (std::size_t i = 1; i < kHeaderReads.size(); ++i) {
        if (kHeaderReads[i].serialized_offset != kHeaderReads[i - 1].serialized_offset + 4u ||
            kHeaderReads[i].call_instruction_offset <= kHeaderReads[i - 1].call_instruction_offset) {
            return false;
        }
    }
    if (kHeaderReads[0].kind != ReadKind::kInt32 ||
            kHeaderReads[0].destination_object_offset != kNoObjectOffset ||
            kHeaderReads[1].kind != ReadKind::kFloat32 ||
            kHeaderReads[1].destination_object_offset != 0x0d4u ||
            kHeaderReads[2].kind != ReadKind::kFloat32 ||
            kHeaderReads[2].destination_object_offset != 0x0d0u ||
            kHeaderReads[3].kind != ReadKind::kUInt32 ||
            kHeaderReads[3].destination_object_offset != kPrimaryCountObjectOffset) return false;

    if (versioned_group_count(kVersionedFanoutGateValue) != kLegacyVersionedGroupCount ||
            versioned_group_count(kVersionedFanoutGateValue + 1) != kModernVersionedGroupCount ||
            versioned_array_object_offset(kModernVersionedGroupCount - 1u) != 0x080u) return false;

    if (kPrimaryFirstVariablePayloadOffset !=
                kPrimaryFirstVariableLengthOffset + kVariableSegmentFramingBytes ||
            kPrimaryRecordFixedBytesExcludingPayload !=
                kPrimaryFirstVariablePayloadOffset +
                    (kPrimaryVariableSegmentCount - 1u) * kVariableSegmentFramingBytes) return false;

    std::size_t bytes = 0;
    if (!primary_record_bytes(0u, 0u, 0u, &bytes) || bytes != 0x4cu ||
            !primary_record_bytes(1u, 2u, 3u, &bytes) || bytes != 0x52u ||
            primary_record_bytes(0u, 0u, 0u, nullptr)) return false;

    if (kSecondaryRecordBytes != 12u || kRootFixedRecordBytes != 53u ||
            kComboTupleBytes != 24u || kFinalTableEntryBytes != 4u) return false;

    if (dynamic_array_object_offset(0u) != 0x014u ||
            dynamic_array_object_offset(7u) != 0x030u ||
            kVersionedArrayBaseObjectOffset != 0x034u ||
            kRootFixedArrayObjectOffset != 0x084u ||
            kPrimaryArrayObjectOffset != 0x088u ||
            kSecondaryArrayObjectOffset != 0x08cu) return false;

    if (kComboTupleReadInstructionOffsets != std::array<std::size_t, 6>{{
                0x29027au, 0x29028eu, 0x2902acu, 0x2902ceu, 0x2902f8u, 0x290328u}} ||
            kComboArrayObjectOffsets != std::array<std::size_t, 3>{{0x094u, 0x09cu, 0x098u}} ||
            kComboAddObjectInstructionOffsets !=
                std::array<std::size_t, 3>{{0x290368u, 0x290384u, 0x2903b4u}}) return false;

    // initWithFile initializes exactly 100 dwords at each base. The topology
    // contract does not claim the primary count is <=100; it only proves the
    // parser's indexed destinations for values that are present.
    if (combo_first_float_destination(0u) != 0x0d8u ||
            combo_first_float_destination(99u) != 0x264u ||
            combo_second_float_destination(0u) != 0x268u ||
            combo_second_float_destination(99u) != 0x3f4u) return false;

    return kPrimaryAddObjectInstructionOffset < kSecondaryCountReadInstructionOffset &&
            kSecondaryAddObjectInstructionOffset < kDynamicOuterCountReadInstructionOffset &&
            kDynamicAddObjectInstructionOffset < kVersionedFanoutGateInstructionOffset &&
            kVersionedAddObjectInstructionOffset < kRootFixedCountReadInstructionOffset &&
            kRootFixedAddObjectInstructionOffset < kComboTupleReadInstructionOffsets.front() &&
            kComboAddObjectInstructionOffsets.back() < kFinalTableReadInstructionOffset &&
            kFinalTableReadInstructionOffset < kFinalStoreInstructionOffset;
}

}  // namespace nevergone::enemy_actions_wbg_topology_evidence
