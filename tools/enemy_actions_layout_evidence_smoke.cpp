#include <cassert>
#include <cstddef>
#include <cstdint>

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

    assert(evidence::kInitArrayCapacity == 128u);
    assert(evidence::kInitArrayPointerOffsets.size() == 36u);
    assert(evidence::kInitArrayPointerOffsets.front() == 0x14u);
    assert(evidence::kInitArrayPointerOffsets.back() == 0xa0u);
    for (std::size_t i = 0; i < evidence::kInitArrayPointerOffsets.size(); ++i) {
        const std::size_t expected = 0x14u + i * 4u;
        assert(evidence::kInitArrayPointerOffsets[i] == expected);
        assert(evidence::is_init_array_pointer_offset(expected));
        assert(evidence::is_constructor_zeroed_pointer_offset(expected));
    }
    assert(!evidence::is_init_array_pointer_offset(0x10u));
    assert(!evidence::is_init_array_pointer_offset(0xa4u));

    const struct {
        std::size_t offset;
        std::uint32_t bits;
    } expected_scalar_writes[] = {
            {0xa4u, 0u},
            {0xa8u, 1u},
            {0xacu, 1u},
            {0xb0u, 1u},
            {0xb4u, 1u},
            {0xb8u, 1u},
            {0xbcu, 1u},
            {0xc0u, 0x18u},
            {0xc8u, 0u},
            {0xccu, 0u},
            {0xd0u, 0u},
            {0xd4u, 0u},
    };
    assert(evidence::kInitScalarWrites.size() == 12u);
    for (std::size_t i = 0; i < evidence::kInitScalarWrites.size(); ++i) {
        assert(evidence::kInitScalarWrites[i].offset == expected_scalar_writes[i].offset);
        assert(evidence::kInitScalarWrites[i].value_bits == expected_scalar_writes[i].bits);
        std::uint32_t bits = 0xffffffffu;
        assert(evidence::init_scalar_word_at(expected_scalar_writes[i].offset, &bits));
        assert(bits == expected_scalar_writes[i].bits);
    }
    std::uint32_t scalar_bits = 0;
    assert(!evidence::init_scalar_word_at(0xc4u, &scalar_bits));
    assert(!evidence::init_scalar_word_at(0xa4u, nullptr));

    assert(evidence::kEarlyHpDataTargetOffsets[0] == 0xd4u);
    assert(evidence::kEarlyHpDataTargetOffsets[1] == 0xd0u);
    assert(evidence::kEarlyHpDataTargetOffsets[2] == 0xc4u);
    assert(evidence::is_early_hpdata_target_offset(0xd4u));
    assert(evidence::is_early_hpdata_target_offset(0xd0u));
    assert(evidence::is_early_hpdata_target_offset(0xc4u));
    assert(!evidence::is_early_hpdata_target_offset(0xc8u));

    static_assert(evidence::kFirstObservedRegionInitialBits == 0x3f800000u);
    static_assert(evidence::kSecondObservedRegionInitialBits == 0x00000000u);

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
