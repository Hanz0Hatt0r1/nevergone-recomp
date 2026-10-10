#include <cassert>
#include <cstddef>
#include <cstdint>
#include <limits>

#include "enemy_actions_wbg_topology_evidence.h"

int main() {
    namespace evidence = nevergone::enemy_actions_wbg_topology_evidence;

    static_assert(evidence::kLoadWbgFileInstructionOffset == 0x28f430u);
    static_assert(evidence::kHeaderBytes == 0x10u);
    assert(evidence::topology_is_consistent());

    assert(evidence::kHeaderReads.size() == 4u);
    assert(evidence::kHeaderReads[0].serialized_offset == 0x00u);
    assert(evidence::kHeaderReads[0].kind == evidence::ReadKind::kInt32);
    assert(evidence::kHeaderReads[0].destination_object_offset == evidence::kNoObjectOffset);
    assert(evidence::kHeaderReads[0].call_instruction_offset == 0x28f4a0u);
    assert(evidence::kHeaderReads[1].serialized_offset == 0x04u);
    assert(evidence::kHeaderReads[1].destination_object_offset == 0x0d4u);
    assert(evidence::kHeaderReads[2].serialized_offset == 0x08u);
    assert(evidence::kHeaderReads[2].destination_object_offset == 0x0d0u);
    assert(evidence::kHeaderReads[3].serialized_offset == 0x0cu);
    assert(evidence::kHeaderReads[3].kind == evidence::ReadKind::kUInt32);
    assert(evidence::kHeaderReads[3].destination_object_offset == 0x0c4u);

    static_assert(evidence::kVersionedFanoutGateValue == 0x68);
    static_assert(evidence::kVersionedFanoutGateInstructionOffset == 0x28fc5cu);
    assert(evidence::versioned_group_count(0x67) == 6u);
    assert(evidence::versioned_group_count(0x68) == 6u);
    assert(evidence::versioned_group_count(0x69) == 20u);

    static_assert(evidence::kPrimaryCountObjectOffset == 0x0c4u);
    static_assert(evidence::kPrimaryArrayObjectOffset == 0x088u);
    static_assert(evidence::kPrimaryAddObjectInstructionOffset == 0x28f820u);
    static_assert(evidence::kPrimaryFloatCount == 12u);
    static_assert(evidence::kPrimaryBoolSerializedOffset == 0x34u);
    static_assert(evidence::kPrimarySecondIntSerializedOffset == 0x35u);
    static_assert(evidence::kPrimaryPostIntFloatSerializedOffset == 0x39u);
    static_assert(evidence::kPrimaryFirstVariableLengthOffset == 0x3du);
    static_assert(evidence::kPrimaryFirstVariablePayloadOffset == 0x42u);
    static_assert(evidence::kVariableSegmentFramingBytes == 5u);
    static_assert(evidence::kPrimaryVariableSegmentCount == 3u);
    static_assert(evidence::kPrimaryRecordFixedBytesExcludingPayload == 0x4cu);

    std::size_t record_bytes = 0;
    assert(evidence::primary_record_bytes(0, 0, 0, &record_bytes));
    assert(record_bytes == 0x4cu);
    assert(evidence::primary_record_bytes(3, 5, 7, &record_bytes));
    assert(record_bytes == 0x5bu);
    assert(!evidence::primary_record_bytes(
            std::numeric_limits<std::size_t>::max(), 0, 0, &record_bytes));
    assert(!evidence::primary_record_bytes(0, 0, 0, nullptr));

    static_assert(evidence::kSecondaryCountReadInstructionOffset == 0x28f842u);
    static_assert(evidence::kSecondaryRecordBytes == 0x0cu);
    static_assert(evidence::kSecondaryArrayObjectOffset == 0x08cu);
    static_assert(evidence::kSecondaryAddObjectInstructionOffset == 0x28f8beu);

    static_assert(evidence::kDynamicOuterCountReadInstructionOffset == 0x28f8dcu);
    static_assert(evidence::kDynamicInnerCountReadInstructionOffset == 0x28f904u);
    static_assert(evidence::kDynamicArrayBaseObjectOffset == 0x014u);
    static_assert(evidence::kDynamicAddObjectInstructionOffset == 0x28fc3eu);
    assert(evidence::dynamic_array_object_offset(0u) == 0x14u);
    assert(evidence::dynamic_array_object_offset(7u) == 0x30u);

    static_assert(evidence::kVersionedGroupCountReadInstructionOffset == 0x28fc7cu);
    static_assert(evidence::kVersionedArrayBaseObjectOffset == 0x034u);
    static_assert(evidence::kVersionedAddObjectInstructionOffset == 0x28ffe8u);
    assert(evidence::versioned_array_object_offset(0u) == 0x34u);
    assert(evidence::versioned_array_object_offset(5u) == 0x48u);
    assert(evidence::versioned_array_object_offset(19u) == 0x80u);

    static_assert(evidence::kRootFixedCountReadInstructionOffset == 0x290018u);
    static_assert(evidence::kRootFixedRecordBytes == 0x35u);
    static_assert(evidence::kRootFixedArrayObjectOffset == 0x084u);
    static_assert(evidence::kRootFixedAddObjectInstructionOffset == 0x290222u);

    static_assert(evidence::kComboTupleBytes == 0x18u);
    assert((evidence::kComboArrayObjectOffsets ==
            std::array<std::size_t, 3>{{0x94u, 0x9cu, 0x98u}}));
    assert((evidence::kComboAddObjectInstructionOffsets ==
            std::array<std::size_t, 3>{{0x290368u, 0x290384u, 0x2903b4u}}));
    assert((evidence::kComboTupleReadInstructionOffsets ==
            std::array<std::size_t, 6>{{
                    0x29027au, 0x29028eu, 0x2902acu,
                    0x2902ceu, 0x2902f8u, 0x290328u}}));

    static_assert(evidence::kFinalTableEntryBytes == 4u);
    static_assert(evidence::kFinalTableReadInstructionOffset == 0x2903f2u);
    static_assert(evidence::kFinalPrimaryArrayObjectOffset == 0x088u);
    static_assert(evidence::kFinalActionFrameDestinationOffset == 0x05cu);
    static_assert(evidence::kFinalStoreInstructionOffset == 0x29041au);

    return 0;
}
