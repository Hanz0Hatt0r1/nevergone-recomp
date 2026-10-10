#include "enemy_actions_load_wbg_evidence.h"

namespace nevergone::enemy_actions_load_wbg_evidence {

const std::array<Callsite, 7> kPreHpDataCallsites{{
    {0x28f44au, kAppParametersSharedInstructionOffset, false},
    {0x28f44eu, kFileUtilsSharedInstructionOffset, false},
    {0x28f45cu, kCcStringGetCStringInstructionOffset, false},
    {0x28f466u, kFullPathForFilenameInstructionOffset, true},
    {0x28f46au, kCcStringCreateFromStdStringInstructionOffset, false},
    {0x28f478u, kCcStringGetCStringInstructionOffset, false},
    {0x28f47cu, kHpDataCreateWithContentsOfFileInstructionOffset, false},
}};

const std::array<Callsite, 4> kFirstNonNullHpDataCallsites{{
    {0x28f4a0u, 0x2c658au, false},  // HPData::getBytes(int&, HPRange)
    {0x28f4b4u, 0x2c65ceu, false},  // HPData::getBytes(float&, HPRange) -> this+0xd4
    {0x28f4cau, 0x2c65ceu, false},  // HPData::getBytes(float&, HPRange) -> this+0xd0
    {0x28f4e2u, 0x2c65acu, false},  // HPData::getBytes(unsigned&, HPRange) -> this+0xc4
}};

bool pre_hpdata_sequence_is_consistent() {
    if (kPreHpDataCallsites.front().call_instruction_offset <= kLoadWbgFileInstructionOffset) {
        return false;
    }
    for (std::size_t i = 1; i < kPreHpDataCallsites.size(); ++i) {
        if (kPreHpDataCallsites[i - 1].call_instruction_offset >=
                kPreHpDataCallsites[i].call_instruction_offset) {
            return false;
        }
    }
    if (!kPreHpDataCallsites[3].virtual_dispatch ||
            kPreHpDataCallsites[3].target_instruction_offset !=
                    kFullPathForFilenameInstructionOffset) {
        return false;
    }
    if (kPreHpDataCallsites.back().target_instruction_offset !=
            kHpDataCreateWithContentsOfFileInstructionOffset) {
        return false;
    }
    return kPreHpDataCallsites.back().call_instruction_offset <
            kHpDataNullCompareInstructionOffset;
}

bool singleton_bootstrap_is_environment_dependent() {
    // sharedAppParameters may allocate and invoke AppParameters::init. The file
    // utils singleton may allocate CCFileUtilsAndroid, invoke its virtual init,
    // query getApkPath and construct a ZipFile. A future dynamic loadWBG probe
    // must therefore reuse/establish this state rather than assuming a leaf call.
    return kAppParametersAllocationBytes == 0x434u &&
            kAppParametersInitVirtualSlotOffset == 0x24u &&
            kAppParametersInitInstructionOffset == 0x291bb8u &&
            kFileUtilsAndroidAllocationBytes == 0x3cu &&
            kFileUtilsAndroidInitVirtualSlotOffset == 0x78u &&
            kFileUtilsAndroidInitInstructionOffset == 0x534ca8u &&
            kGetApkPathInstructionOffset == 0x535c88u &&
            kZipFileConstructorInstructionOffset == 0x5410e4u;
}

bool hpdata_null_path_skips_typed_reads() {
    if (!(kHpDataNullCompareInstructionOffset < kHpDataNullBranchInstructionOffset &&
            kHpDataNullBranchInstructionOffset <
                    kFirstNonNullHpDataCallsites.front().call_instruction_offset)) {
        return false;
    }
    if (kHpDataNullExitInstructionOffset <=
            kFirstNonNullHpDataCallsites.back().call_instruction_offset) {
        return false;
    }
    if (kFunctionReturnInstructionOffset < kHpDataNullExitInstructionOffset) {
        return false;
    }
    return true;
}

}  // namespace nevergone::enemy_actions_load_wbg_evidence
