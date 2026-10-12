#include <cassert>
#include <cstdint>
#include <cstring>
#include <vector>

#include "enemy_actions_wbg_combo_section.h"
#include "hp_data_reader.h"
#include "me_actions_combo_boundaries.h"

namespace combo = nevergone::enemy_actions_wbg_combo_section;
namespace me = nevergone::me_actions_combo_boundaries;
namespace hp = nevergone::hp_data;

namespace {

void append_i32(std::vector<std::uint8_t>* bytes, std::int32_t value) {
    std::uint32_t bits = 0;
    std::memcpy(&bits, &value, sizeof(bits));
    for (int shift = 0; shift < 32; shift += 8) {
        bytes->push_back(static_cast<std::uint8_t>((bits >> shift) & 0xffu));
    }
}

void append_f32(std::vector<std::uint8_t>* bytes, float value) {
    std::uint32_t bits = 0;
    std::memcpy(&bits, &value, sizeof(bits));
    for (int shift = 0; shift < 32; shift += 8) {
        bytes->push_back(static_cast<std::uint8_t>((bits >> shift) & 0xffu));
    }
}

void append_tuple(
        std::vector<std::uint8_t>* bytes,
        std::int32_t a,
        std::int32_t b,
        std::int32_t c,
        std::int32_t d) {
    append_i32(bytes, a);
    append_i32(bytes, b);
    append_i32(bytes, c);
    append_i32(bytes, d);
    append_f32(bytes, 1.0f);
    append_f32(bytes, 2.0f);
}

}  // namespace

int main() {
    std::vector<std::uint8_t> bytes;
    append_tuple(&bytes, 0, 0, 0, 0);  // both run states remain 1
    append_tuple(&bytes, 0, 0, 0, 1);  // fourth+1 changes: close player +0xb0
    append_tuple(&bytes, 0, 2, 0, 1);  // second+1 changes: close player +0xb4
    append_tuple(&bytes, 0, 2, 0, 1);  // final tuple closes both runs

    hp::Reader reader(std::move(bytes));
    combo::Block block;
    assert(combo::parse(reader, 0u, 4u, &block));
    assert(block.tuples.size() == 4u);

    const auto boundaries = me::derive(block);
    assert(me::structurally_matches_existing_section_f_derivations(block, boundaries));
    assert(boundaries.array_b0_values.size() == 2u);
    assert(boundaries.array_b4_values.size() == 2u);

    const auto& b0_first = boundaries.array_b0_values[0];
    assert(b0_first.target_array_offset == 0xb0u);
    assert(b0_first.source_tuple_index == 1u);
    assert(b0_first.field_14 == 0);
    assert(b0_first.field_18 == 1);
    assert(b0_first.field_1c == 1);

    const auto& b0_last = boundaries.array_b0_values[1];
    assert(b0_last.source_tuple_index == 3u);
    assert(b0_last.field_14 == 2);
    assert(b0_last.field_18 == 3);
    assert(b0_last.field_1c == 2);

    const auto& b4_first = boundaries.array_b4_values[0];
    assert(b4_first.target_array_offset == 0xb4u);
    assert(b4_first.source_tuple_index == 2u);
    assert(b4_first.field_14 == 0);
    assert(b4_first.field_18 == 2);
    assert(b4_first.field_1c == 1);

    const auto& b4_last = boundaries.array_b4_values[1];
    assert(b4_last.source_tuple_index == 3u);
    assert(b4_last.field_14 == 3);
    assert(b4_last.field_18 == 3);
    assert(b4_last.field_1c == 3);

    // Empty Section-F input stays empty; the live playback layer can fail
    // closed instead of fabricating a boundary object.
    combo::Block empty;
    const auto empty_boundaries = me::derive(empty);
    assert(empty_boundaries.array_b0_values.empty());
    assert(empty_boundaries.array_b4_values.empty());
    return 0;
}
