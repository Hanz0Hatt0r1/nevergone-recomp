#include "enemy_actions_layout_evidence.h"

#include <algorithm>

namespace nevergone::enemy_actions_layout_evidence {

const std::array<std::size_t, 38> kConstructorZeroedPointerOffsets{{
    0x014u, 0x018u, 0x01cu, 0x020u, 0x024u, 0x028u, 0x02cu, 0x030u,
    0x034u, 0x038u, 0x03cu, 0x040u, 0x044u, 0x048u, 0x04cu, 0x050u,
    0x054u, 0x058u, 0x05cu, 0x060u, 0x064u, 0x068u, 0x06cu, 0x070u,
    0x074u, 0x078u, 0x07cu, 0x080u,
    0x084u, 0x088u, 0x08cu, 0x090u, 0x094u, 0x098u, 0x09cu, 0x0a0u,
    0x0c8u, 0x0ccu,
}};

const std::array<std::size_t, 36> kInitArrayPointerOffsets{{
    0x014u, 0x018u, 0x01cu, 0x020u, 0x024u, 0x028u, 0x02cu, 0x030u,
    0x034u, 0x038u, 0x03cu, 0x040u, 0x044u, 0x048u, 0x04cu, 0x050u,
    0x054u, 0x058u, 0x05cu, 0x060u, 0x064u, 0x068u, 0x06cu, 0x070u,
    0x074u, 0x078u, 0x07cu, 0x080u,
    0x084u, 0x088u, 0x08cu, 0x090u, 0x094u, 0x098u, 0x09cu, 0x0a0u,
}};

const std::array<InitScalarWrite, 12> kInitScalarWrites{{
    {0x0a4u, 0x00000000u},
    {0x0a8u, 0x00000001u},
    {0x0acu, 0x00000001u},
    {0x0b0u, 0x00000001u},
    {0x0b4u, 0x00000001u},
    {0x0b8u, 0x00000001u},
    {0x0bcu, 0x00000001u},
    {0x0c0u, 0x00000018u},
    {0x0c8u, 0x00000000u},
    {0x0ccu, 0x00000000u},
    {0x0d0u, 0x00000000u},
    {0x0d4u, 0x00000000u},
}};

const std::array<std::size_t, 3> kEarlyHpDataTargetOffsets{{
    0x0d4u,
    0x0d0u,
    0x0c4u,
}};

namespace {

bool observed_region_element_offset(
        std::size_t base,
        std::size_t index,
        std::size_t* out) {
    if (out == nullptr || index >= kObservedSampleCount) return false;
    *out = base + index * kObservedElementBytes;
    return true;
}

bool four_byte_aligned(std::size_t value) {
    return (value & 0x3u) == 0u;
}

}  // namespace

bool is_constructor_zeroed_pointer_offset(std::size_t offset) {
    return std::binary_search(
            kConstructorZeroedPointerOffsets.begin(),
            kConstructorZeroedPointerOffsets.end(),
            offset);
}

bool is_early_hpdata_target_offset(std::size_t offset) {
    return std::find(
            kEarlyHpDataTargetOffsets.begin(),
            kEarlyHpDataTargetOffsets.end(),
            offset) != kEarlyHpDataTargetOffsets.end();
}

bool is_init_array_pointer_offset(std::size_t offset) {
    return std::binary_search(
            kInitArrayPointerOffsets.begin(),
            kInitArrayPointerOffsets.end(),
            offset);
}

bool init_scalar_word_at(std::size_t offset, std::uint32_t* out_value_bits) {
    if (out_value_bits == nullptr) return false;
    const auto it = std::find_if(
            kInitScalarWrites.begin(),
            kInitScalarWrites.end(),
            [offset](const InitScalarWrite& write) { return write.offset == offset; });
    if (it == kInitScalarWrites.end()) return false;
    *out_value_bits = it->value_bits;
    return true;
}

bool first_observed_region_element_offset(std::size_t index, std::size_t* out) {
    return observed_region_element_offset(kFirstObservedRegionOffset, index, out);
}

bool second_observed_region_element_offset(std::size_t index, std::size_t* out) {
    return observed_region_element_offset(kSecondObservedRegionOffset, index, out);
}

bool observed_regions_are_consistent() {
    const std::size_t first_end =
            kFirstObservedRegionOffset + kObservedSampleCount * kObservedElementBytes;
    const std::size_t second_end =
            kSecondObservedRegionOffset + kObservedSampleCount * kObservedElementBytes;
    if (first_end != kSecondObservedRegionOffset ||
            second_end != kMinimumObservedObjectBytes) {
        return false;
    }

    if (!four_byte_aligned(kScalarDefaultsBeginOffset) ||
            !four_byte_aligned(kScalarDefaultsEndOffset) ||
            !four_byte_aligned(kFirstObservedRegionOffset) ||
            !four_byte_aligned(kSecondObservedRegionOffset)) {
        return false;
    }

    if (!std::is_sorted(
                kConstructorZeroedPointerOffsets.begin(),
                kConstructorZeroedPointerOffsets.end()) ||
            !std::is_sorted(kInitArrayPointerOffsets.begin(), kInitArrayPointerOffsets.end())) {
        return false;
    }
    for (const std::size_t offset : kConstructorZeroedPointerOffsets) {
        if (!four_byte_aligned(offset) || offset >= kMinimumObservedObjectBytes) {
            return false;
        }
    }
    for (const std::size_t offset : kInitArrayPointerOffsets) {
        if (!four_byte_aligned(offset) ||
                !is_constructor_zeroed_pointer_offset(offset) ||
                offset < 0x014u || offset > 0x0a0u) {
            return false;
        }
    }
    if (kInitArrayPointerOffsets.front() != 0x014u ||
            kInitArrayPointerOffsets.back() != 0x0a0u ||
            kInitArrayCapacity != 0x80u) {
        return false;
    }

    std::size_t previous_scalar_offset = 0;
    bool have_previous_scalar = false;
    for (const InitScalarWrite& write : kInitScalarWrites) {
        if (!four_byte_aligned(write.offset) ||
                write.offset < kScalarDefaultsBeginOffset ||
                write.offset > kScalarDefaultsEndOffset ||
                (have_previous_scalar && write.offset <= previous_scalar_offset)) {
            return false;
        }
        previous_scalar_offset = write.offset;
        have_previous_scalar = true;
    }

    for (const std::size_t offset : kEarlyHpDataTargetOffsets) {
        if (!four_byte_aligned(offset) || offset >= kFirstObservedRegionOffset) {
            return false;
        }
    }
    return true;
}

}  // namespace nevergone::enemy_actions_layout_evidence
