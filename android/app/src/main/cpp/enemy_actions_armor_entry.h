#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

#include "enemy_actions_wbg_document.h"

namespace nevergone::enemy_actions_armor_entry {

constexpr std::size_t kArmorSlotCount = 8u;
constexpr std::size_t kArmorSpriteBaseOffset = 0x130u;
constexpr std::size_t kArmorSpriteStride = 4u;
constexpr std::size_t kArmorArrayBaseOffset = 0x14u;

struct SlotResult {
    std::size_t armor_id = 0;
    std::size_t sprite_object_offset = 0;
    std::size_t array_object_offset = 0;

    // Native first hides every non-null sprite in system+0x130..+0x14c.
    bool sprite_present = false;
    bool should_set_visible_false = false;

    // Project-owned representation of EnemyActionsData::armorArrayWithID(id).
    // The native object owns all array slots; a serialized Section-C group may
    // simply be absent from our bounded parsed representation.
    bool serialized_group_available = false;
    std::size_t serialized_record_count = 0;
    bool native_would_scan_records = false;

    // armorFDWithID(id,index) records are visited in ascending index order. The
    // first record whose ActionFrameData+0x74 equals current_frame is the one
    // that reaches the recovered armor resource/property body.
    bool matching_record_available = false;
    std::size_t selected_record_index = 0;
    std::int32_t selected_field_74 = 0;
};

struct Result {
    std::array<SlotResult, kArmorSlotCount> slots{};
};

inline Result apply(
        const enemy_actions_wbg_document::Document& document,
        std::int32_t current_frame_294,
        const std::array<bool, kArmorSlotCount>& armor_sprite_present) {
    Result result;

    for (std::size_t armor_id = 0; armor_id < kArmorSlotCount; ++armor_id) {
        auto& slot = result.slots[armor_id];
        slot.armor_id = armor_id;
        slot.sprite_object_offset = kArmorSpriteBaseOffset + armor_id * kArmorSpriteStride;
        slot.array_object_offset = kArmorArrayBaseOffset + armor_id * kArmorSpriteStride;
        slot.sprite_present = armor_sprite_present[armor_id];
        slot.should_set_visible_false = slot.sprite_present;

        if (armor_id >= document.nested_block.groups.size()) continue;

        slot.serialized_group_available = true;
        const auto& records = document.nested_block.groups[armor_id].records;
        slot.serialized_record_count = records.size();
        slot.native_would_scan_records = !records.empty();

        for (std::size_t record_index = 0; record_index < records.size(); ++record_index) {
            const auto& record = records[record_index];
            if (record.first_i32 != current_frame_294) continue;

            slot.matching_record_available = true;
            slot.selected_record_index = record_index;
            slot.selected_field_74 = record.first_i32;
            break;
        }
    }

    return result;
}

}  // namespace nevergone::enemy_actions_armor_entry
