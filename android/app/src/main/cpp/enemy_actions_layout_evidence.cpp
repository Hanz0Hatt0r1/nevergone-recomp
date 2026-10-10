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
                kConstructorZeroedPointerOffsets.end())) {
        return false;
    }
    for (const std::size_t offset : kConstructorZeroedPointerOffsets) {
        if (!four_byte_aligned(offset) || offset >= kMinimumObservedObjectBytes) {
            return false;
        }
    }
    for (const std::size_t offset : kEarlyHpDataTargetOffsets) {
        if (!four_byte_aligned(offset) || offset >= kFirstObservedRegionOffset) {
            return false;
        }
    }
    return true;
}

}  // namespace nevergone::enemy_actions_layout_evidence
