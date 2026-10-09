#include <cassert>
#include <cstdint>
#include <cstring>
#include <vector>

#include "game_levels_scene_prefix.h"
#include "hp_data_reader.h"

namespace {
void u32(std::vector<std::uint8_t>& b, std::uint32_t v) {
    for (int s = 0; s < 32; s += 8) b.push_back(static_cast<std::uint8_t>(v >> s));
}
void f32(std::vector<std::uint8_t>& b, float v) {
    std::uint32_t x = 0; std::memcpy(&x, &v, 4); u32(b, x);
}
std::vector<std::uint8_t> fixture(std::int32_t version) {
    std::vector<std::uint8_t> b;
    u32(b, static_cast<std::uint32_t>(version)); u32(b, 1);
    u32(b, 1); b.push_back(0); b.push_back('s'); f32(b, 1); f32(b, 2); u32(b, 1);
    f32(b, 3); u32(b, 1);
    u32(b, 7); u32(b, 1); b.push_back(0); b.push_back('o');
    for (int i = 0; i < 5; ++i) f32(b, static_cast<float>(i));
    u32(b, 8); b.push_back(1); b.push_back(0);
    return b;
}
}

int main() {
    using namespace nevergone::game_levels_scene_prefix;

    auto old_bytes = fixture(2);
    nevergone::hp_data::Reader old_reader(old_bytes);
    FirstObjectVersionedList old_result;
    assert(parse_first_object_versioned_list(old_reader, &old_result));
    assert(!old_result.present);
    assert(old_result.value_count == 0);
    assert(old_result.values.empty());
    assert(old_result.bytes_consumed == old_bytes.size());

    auto new_bytes = fixture(3);
    const std::size_t core_end = new_bytes.size();
    u32(new_bytes, 2); u32(new_bytes, 0x11223344u); u32(new_bytes, 0xaabbccddu);
    nevergone::hp_data::Reader new_reader(new_bytes);
    FirstObjectVersionedList new_result;
    assert(parse_first_object_versioned_list(new_reader, &new_result));
    assert(new_result.present);
    assert(new_result.value_count == 2);
    assert(new_result.values.size() == 2);
    assert(new_result.values[0] == 0x11223344u);
    assert(new_result.values[1] == 0xaabbccddu);
    assert(new_result.bytes_consumed == core_end + 12u);

    auto truncated = fixture(3);
    u32(truncated, 3); u32(truncated, 1); u32(truncated, 2);
    nevergone::hp_data::Reader truncated_reader(truncated);
    FirstObjectVersionedList unchanged;
    unchanged.present = true;
    unchanged.value_count = 99;
    unchanged.values = {9};
    unchanged.bytes_consumed = 123;
    assert(!parse_first_object_versioned_list(truncated_reader, &unchanged));
    assert(unchanged.value_count == 99);
    assert(unchanged.values == std::vector<std::uint32_t>{9});
    assert(unchanged.bytes_consumed == 123u);

    assert(!parse_first_object_versioned_list(new_reader, nullptr));
    return 0;
}
