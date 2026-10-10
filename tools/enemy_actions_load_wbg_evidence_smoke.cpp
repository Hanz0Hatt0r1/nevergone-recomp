#include <cassert>
#include <cstddef>

#include "enemy_actions_load_wbg_evidence.h"

int main() {
    namespace evidence = nevergone::enemy_actions_load_wbg_evidence;

    static_assert(evidence::kLoadWbgFileInstructionOffset == 0x28f430u);
    static_assert(evidence::kAppParametersSharedInstructionOffset == 0x2938bcu);
    static_assert(evidence::kFileUtilsSharedInstructionOffset == 0x534c30u);
    static_assert(evidence::kFullPathForFilenameInstructionOffset == 0x532da4u);
    static_assert(evidence::kCcStringGetCStringInstructionOffset == 0x519e8eu);
    static_assert(evidence::kCcStringCreateFromStdStringInstructionOffset == 0x519f8au);
    static_assert(evidence::kHpDataCreateWithContentsOfFileInstructionOffset == 0x2c652cu);

    assert(evidence::kPreHpDataCallsites.size() == 7u);
    assert(evidence::pre_hpdata_sequence_is_consistent());
    assert(evidence::kPreHpDataCallsites[0].call_instruction_offset == 0x28f44au);
    assert(evidence::kPreHpDataCallsites[1].call_instruction_offset == 0x28f44eu);
    assert(evidence::kPreHpDataCallsites[2].call_instruction_offset == 0x28f45cu);
    assert(evidence::kPreHpDataCallsites[3].call_instruction_offset == 0x28f466u);
    assert(evidence::kPreHpDataCallsites[3].virtual_dispatch);
    assert(evidence::kPreHpDataCallsites[4].call_instruction_offset == 0x28f46au);
    assert(evidence::kPreHpDataCallsites[5].call_instruction_offset == 0x28f478u);
    assert(evidence::kPreHpDataCallsites[6].call_instruction_offset == 0x28f47cu);

    assert(evidence::singleton_bootstrap_is_environment_dependent());
    assert(evidence::kAppParametersAllocationBytes == 0x434u);
    assert(evidence::kAppParametersInitVirtualSlotOffset == 0x24u);
    assert(evidence::kFileUtilsAndroidAllocationBytes == 0x3cu);
    assert(evidence::kFileUtilsAndroidInitVirtualSlotOffset == 0x78u);
    assert(evidence::kGetApkPathInstructionOffset == 0x535c88u);
    assert(evidence::kZipFileConstructorInstructionOffset == 0x5410e4u);

    assert(evidence::virtual_dispatch_slots_are_consistent());
    assert(evidence::kAppParametersVtableSymbolValue == 0x8e3f60u);
    assert(evidence::kAppParametersInitVtableEntryValue == 0x291bb9u);
    assert(evidence::kFileUtilsAndroidVtableSymbolValue == 0x926098u);
    assert(evidence::kFileUtilsAndroidInitVtableEntryValue == 0x534ca9u);
    assert(evidence::kFullPathForFilenameVirtualSlotOffset == 0x18u);
    assert(evidence::kFullPathForFilenameVtableEntryValue == 0x532da5u);

    assert(evidence::hpdata_null_path_skips_typed_reads());
    assert(evidence::kHpDataNullCompareInstructionOffset == 0x28f484u);
    assert(evidence::kHpDataNullBranchInstructionOffset == 0x28f486u);
    assert(evidence::kHpDataNullExitInstructionOffset == 0x290422u);
    assert(evidence::kFunctionReturnInstructionOffset == 0x290438u);

    assert(evidence::kFirstNonNullHpDataCallsites.size() == 4u);
    assert(evidence::kFirstNonNullHpDataCallsites[0].call_instruction_offset == 0x28f4a0u);
    assert(evidence::kFirstNonNullHpDataCallsites[0].target_instruction_offset == 0x2c658au);
    assert(evidence::kFirstNonNullHpDataCallsites[1].call_instruction_offset == 0x28f4b4u);
    assert(evidence::kFirstNonNullHpDataCallsites[1].target_instruction_offset == 0x2c65ceu);
    assert(evidence::kFirstNonNullHpDataCallsites[2].call_instruction_offset == 0x28f4cau);
    assert(evidence::kFirstNonNullHpDataCallsites[2].target_instruction_offset == 0x2c65ceu);
    assert(evidence::kFirstNonNullHpDataCallsites[3].call_instruction_offset == 0x28f4e2u);
    assert(evidence::kFirstNonNullHpDataCallsites[3].target_instruction_offset == 0x2c65acu);

    return 0;
}
