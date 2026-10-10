#include <cassert>
#include <cstddef>

#include "enemy_actions_layout_evidence.h"

int main() {
    namespace evidence = nevergone::enemy_actions_layout_evidence;

    static_assert(evidence::kConstructorInstructionOffset == 0x28f394u);
    static_assert(evidence::kLoadWbgFileInstructionOffset == 0x28f430u);
    static_assert(evidence::kInitWithFileInstructionOffset == 0x29043cu);
    static_assert(evidence::kLoadWbgCallFromInitInstructionOffset == 0x29054au);
    static_assert(evidence::kHpDataCreateWithContentsOfFileInstructionOffset == 0x2c652cu);

    assert(evidence::observed_regions_are_consistent());
    assert(evidence::kConstructorZeroedPointerOffsets.size() == 38u);
    assert(evidence::is_constructor_zeroed_pointer_offset(0x14u));
    assert(evidence::is_constructor_zeroed_pointer_offset(0x30u));
    assert(evidence::is_constructor_zeroed_pointer_offset(0x34u));
    assert(evidence::is_constructor_zeroed_pointer_offset(0x80u));
    assert(evidence::is_constructor_zeroed_pointer_offset(0x84u));
    assert(evidence::is_constructor_zeroed_pointer_offset(0xa0u));
    assert(evidence::is_constructor_zeroed_pointer_offset(0xc8u));
    assert(evidence::is_constructor_zeroed_pointer_offset(0xccu));
    assert(!evidence::is_constructor_zeroed_pointer_offset(0x10u));
    assert(!evidence::is_constructor_zeroed_pointer_offset(0xc4u));
    assert(!evidence::is_constructor_zeroed_pointer_offset(0xd0u));

    assert(evidence::kEarlyHpDataTargetOffsets[0] == 0xd4u);
    assert(evidence::kEarlyHpDataTargetOffsets[1] == 0xd0u);
    assert(evidence::kEarlyHpDataTargetOffsets[2] == 0xc4u);
    assert(evidence::is_early_hpdata_target_offset(0xd4u));
    assert(evidence::is_early_hpdata_target_offset(0xd0u));
    assert(evidence::is_early_hpdata_target_offset(0xc4u));
    assert(!evidence::is_early_hpdata_target_offset(0xc8u));

    std::size_t offset = 0;
    assert(evidence::first_observed_region_element_offset(0u, &offset));
    assert(offset == 0xd8u);
    assert(evidence::first_observed_region_element_offset(99u, &offset));
    assert(offset == 0x264u);
    assert(!evidence::first_observed_region_element_offset(100u, &offset));
    assert(!evidence::first_observed_region_element_offset(0u, nullptr));

    assert(evidence::second_observed_region_element_offset(0u, &offset));
    assert(offset == 0x268u);
    assert(evidence::second_observed_region_element_offset(99u, &offset));
    assert(offset == 0x3f4u);
    assert(!evidence::second_observed_region_element_offset(100u, &offset));

    static_assert(evidence::kFirstObservedRegionOffset +
                          evidence::kObservedSampleCount * evidence::kObservedElementBytes ==
                  evidence::kSecondObservedRegionOffset);
    static_assert(evidence::kSecondObservedRegionOffset +
                          evidence::kObservedSampleCount * evidence::kObservedElementBytes ==
                  evidence::kMinimumObservedObjectBytes);

    return 0;
}
