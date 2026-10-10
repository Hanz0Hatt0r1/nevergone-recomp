#include "enemy_actions_wbg_prefix.h"

#include <cassert>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <vector>

namespace {

void append_u8(std::vector<std::uint8_t>* out, std::uint8_t value) {
    out->push_back(value);
}

void append_u32(std::vector<std::uint8_t>* out, std::uint32_t value) {
    out->push_back(static_cast<std::uint8_t>(value));
    out->push_back(static_cast<std::uint8_t>(value >> 8u));
    out->push_back(static_cast<std::uint8_t>(value >> 16u));
    out->push_back(static_cast<std::uint8_t>(value >> 24u));
}

void append_i32(std::vector<std::uint8_t>* out, std::int32_t value) {
    append_u32(out, static_cast<std::uint32_t>(value));
}

void append_f32(std::vector<std::uint8_t>* out, float value) {
    std::uint32_t bits = 0;
    std::memcpy(&bits, &value, sizeof(bits));
    append_u32(out, bits);
}

void append_record(
        std::vector<std::uint8_t>* out,
        std::int32_t first_i32,
        float first_float,
        bool first_bool) {
    append_i32(out, first_i32);
    for (int i = 0; i < 12; ++i) {
        append_f32(out, first_float + static_cast<float>(i));
    }
    append_u8(out, first_bool ? 1u : 0u);
}

}  // namespace

int main() {
    namespace wbg = nevergone::enemy_actions_wbg_prefix;

    std::vector<std::uint8_t> bytes;
    append_i32(&bytes, 2);
    append_record(&bytes, 101, 1.25f, true);
    append_record(&bytes, -202, 20.5f, false);

    wbg::FixedTailActionFrameBlock block;
    assert(wbg::parse_fixed_tail_action_frame_block(
            nevergone::hp_data::Reader(bytes), 0, &block));
    assert(block.count_i32 == 2);
    assert(block.records.size() == 2u);
    assert(block.bytes_consumed ==
           wbg::kFixedTailBlockHeaderBytes + 2u * wbg::kFixedTailActionFrameBytes);
    assert(block.bytes_consumed == bytes.size());

    assert(block.records[0].first_i32 == 101);
    assert(block.records[0].first_bool);
    assert(block.records[1].first_i32 == -202);
    assert(!block.records[1].first_bool);
    for (int i = 0; i < 12; ++i) {
        assert(std::fabs(block.records[0].float_values[static_cast<std::size_t>(i)] -
                         (1.25f + static_cast<float>(i))) < 0.0001f);
        assert(std::fabs(block.records[1].float_values[static_cast<std::size_t>(i)] -
                         (20.5f + static_cast<float>(i))) < 0.0001f);
    }

    for (std::int32_t count : {0, -4}) {
        std::vector<std::uint8_t> count_only;
        append_i32(&count_only, count);
        wbg::FixedTailActionFrameBlock empty;
        assert(wbg::parse_fixed_tail_action_frame_block(
                nevergone::hp_data::Reader(count_only), 0, &empty));
        assert(empty.count_i32 == count);
        assert(empty.records.empty());
        assert(empty.bytes_consumed == wbg::kFixedTailBlockHeaderBytes);
    }

    std::vector<std::uint8_t> truncated;
    append_i32(&truncated, 2);
    append_record(&truncated, 303, 3.0f, true);
    wbg::FixedTailActionFrameBlock unchanged;
    unchanged.count_i32 = 77;
    unchanged.records.push_back({88, {}, false});
    unchanged.bytes_consumed = 99u;
    assert(!wbg::parse_fixed_tail_action_frame_block(
            nevergone::hp_data::Reader(truncated), 0, &unchanged));
    assert(unchanged.count_i32 == 77);
    assert(unchanged.records.size() == 1u);
    assert(unchanged.records[0].first_i32 == 88);
    assert(unchanged.bytes_consumed == 99u);

    return 0;
}
