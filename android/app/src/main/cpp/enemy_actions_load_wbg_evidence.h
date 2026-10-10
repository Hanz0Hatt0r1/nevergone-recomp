#pragma once

#include <array>
#include <cstddef>

namespace nevergone::enemy_actions_load_wbg_evidence {

// Original Never Gone 1.0.9 ARMv7 ELF32/Thumb instruction offsets. These are
// clean-room evidence metadata only; they are not addresses in the rebuilt app.
constexpr std::size_t kLoadWbgFileInstructionOffset = 0x28f430u;

struct Callsite {
    std::size_t call_instruction_offset = 0;
    std::size_t target_instruction_offset = 0;
    bool virtual_dispatch = false;
};

// Calls performed before loadWBGFile knows whether HPData could open the file.
extern const std::array<Callsite, 7> kPreHpDataCallsites;

constexpr std::size_t kItaniumVtableAddressPointBytes = 8u;

constexpr std::size_t kAppParametersSharedInstructionOffset = 0x2938bcu;
constexpr std::size_t kAppParametersConstructorInstructionOffset = 0x293654u;
constexpr std::size_t kAppParametersInitInstructionOffset = 0x291bb8u;
constexpr std::size_t kAppParametersAllocationBytes = 0x434u;
constexpr std::size_t kAppParametersVtableSymbolValue = 0x8e3f60u;
constexpr std::size_t kAppParametersInitVirtualSlotOffset = 0x24u;
constexpr std::size_t kAppParametersInitVtableEntryValue = 0x291bb9u;

constexpr std::size_t kFileUtilsSharedInstructionOffset = 0x534c30u;
constexpr std::size_t kFileUtilsAndroidConstructorInstructionOffset = 0x534c14u;
constexpr std::size_t kFileUtilsAndroidInitInstructionOffset = 0x534ca8u;
constexpr std::size_t kFileUtilsAndroidAllocationBytes = 0x3cu;
constexpr std::size_t kFileUtilsAndroidVtableSymbolValue = 0x926098u;
constexpr std::size_t kFileUtilsAndroidInitVirtualSlotOffset = 0x78u;
constexpr std::size_t kFileUtilsAndroidInitVtableEntryValue = 0x534ca9u;
constexpr std::size_t kGetApkPathInstructionOffset = 0x535c88u;
constexpr std::size_t kZipFileConstructorInstructionOffset = 0x5410e4u;

// CCFileUtilsAndroid is the concrete singleton created by sharedFileUtils(). Its
// vtable address point is the ABI vtable symbol + 8 bytes. loadWBGFile reads the
// +0x18 virtual slot from that address point. The entry is the inherited
// CCFileUtils::fullPathForFilename(char const*) Thumb symbol.
constexpr std::size_t kFullPathForFilenameVirtualSlotOffset = 0x18u;
constexpr std::size_t kFullPathForFilenameInstructionOffset = 0x532da4u;
constexpr std::size_t kFullPathForFilenameVtableEntryValue = 0x532da5u;

constexpr std::size_t kCcStringGetCStringInstructionOffset = 0x519e8eu;
constexpr std::size_t kCcStringCreateFromStdStringInstructionOffset = 0x519f8au;
constexpr std::size_t kHpDataCreateWithContentsOfFileInstructionOffset = 0x2c652cu;

// The null return from HPData::createWithContentsOfFile is checked immediately.
// The taken branch reaches only stack-canary validation and the epilogue, so no
// typed HPData getBytes call is made on this path.
constexpr std::size_t kHpDataNullCompareInstructionOffset = 0x28f484u;
constexpr std::size_t kHpDataNullBranchInstructionOffset = 0x28f486u;
constexpr std::size_t kHpDataNullExitInstructionOffset = 0x290422u;
constexpr std::size_t kFunctionReturnInstructionOffset = 0x290438u;

// First typed HPData reads on the non-null path, retained to make the boundary
// explicit. The target object offsets are separately covered by layout evidence.
extern const std::array<Callsite, 4> kFirstNonNullHpDataCallsites;

bool pre_hpdata_sequence_is_consistent();
bool singleton_bootstrap_is_environment_dependent();
bool virtual_dispatch_slots_are_consistent();
bool hpdata_null_path_skips_typed_reads();

}  // namespace nevergone::enemy_actions_load_wbg_evidence
