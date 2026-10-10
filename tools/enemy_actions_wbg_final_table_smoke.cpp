#include <cassert>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <vector>

#include "enemy_actions_wbg_final_table.h"
#include "hp_data_reader.h"

namespace {

void append_i32(std::vector<std::uint8_t>* bytes, std::int32_t value) {
    const std::uint32_t raw = static_cast<std::uint32_t>(value);
    bytes->push_back(static_cast<std::uint8_t>(raw & 0xffu));
    bytes->push_back(static_cast<std::uint8_t>((raw >> 8u) & 0xffu));
    bytes->push_back(static_cast<std::uint8_t>((raw >> 16u) & 0xffu));
    bytes->push_back(static_cast<std::uint8_t>((raw >> 24u) & 0xffu));
}

}  // namespace

int main() {
    using namespace nevergone;

    std::vector<std::uint8_t> bytes = {0xaa, 0xbb};
    append_i32(&bytes, 2);
    append_i32(&bytes, -4);
    append_i32(&bytes, 0);
    bytes.push_back(0xcc);

    hp_data::Reader reader(bytes);
    enemy_actions_wbg_final_table::Table table;
    assert(enemy_actions_wbg_final_table::parse(reader, 2u, 3u, &table));
    assert(table.primary_record_count == 3u);
    assert(table.entries.size() == 3u);
    assert(table.bytes_consumed == 12u);

    assert(table.entries[0].serialized_value == 2);
    assert(table.entries[0].reciprocal_value == 0.5f);
    assert(table.entries[1].serialized_value == -4);
    assert(table.entries[1].reciprocal_value == -0.25f);
    assert(table.entries[2].serialized_value == 0);
    assert(std::isinf(table.entries[2].reciprocal_value));
    assert(table.entries[2].reciprocal_value > 0.0f);
    for (const auto& entry : table.entries) {
        assert(entry.action_frame_target_offset ==
               enemy_actions_wbg_final_table::kActionFrameReciprocalTargetOffset);
    }

    enemy_actions_wbg_final_table::Table empty;
    assert(enemy_actions_wbg_final_table::parse(reader, reader.size(), 0u, &empty));
    assert(empty.entries.empty());
    assert(empty.bytes_consumed == 0u);

    enemy_actions_wbg_final_table::Table preserved;
    preserved.primary_record_count = 99u;
    preserved.bytes_consumed = 123u;
    preserved.entries.push_back({7, 7.0f, 7u});
    assert(!enemy_actions_wbg_final_table::parse(reader, 2u, 4u, &preserved));
    assert(preserved.primary_record_count == 99u);
    assert(preserved.bytes_consumed == 123u);
    assert(preserved.entries.size() == 1u);
    assert(preserved.entries[0].serialized_value == 7);
    assert(preserved.entries[0].reciprocal_value == 7.0f);
    assert(preserved.entries[0].action_frame_target_offset == 7u);

    assert(!enemy_actions_wbg_final_table::parse(reader, reader.size() + 1u, 0u, &table));
    assert(!enemy_actions_wbg_final_table::parse(reader, 2u, 1u, nullptr));
    return 0;
}
