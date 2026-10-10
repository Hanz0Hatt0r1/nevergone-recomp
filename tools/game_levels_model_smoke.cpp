#include <cassert>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

#include "game_levels_model.h"
#include "hp_data_reader.h"

namespace {
void append_u32(std::vector<std::uint8_t>* out, std::uint32_t value) {
    assert(out != nullptr);
    for (unsigned shift = 0; shift < 32; shift += 8) {
        out->push_back(static_cast<std::uint8_t>(value >> shift));
    }
}
void append_i32(std::vector<std::uint8_t>* out, std::int32_t value) {
    append_u32(out, static_cast<std::uint32_t>(value));
}
void append_f32(std::vector<std::uint8_t>* out, float value) {
    std::uint32_t bits = 0;
    static_assert(sizeof(bits) == sizeof(value));
    std::memcpy(&bits, &value, sizeof(bits));
    append_u32(out, bits);
}
void append_string(std::vector<std::uint8_t>* out, const std::string& value, std::uint8_t gap) {
    append_u32(out, static_cast<std::uint32_t>(value.size()));
    out->push_back(gap);
    out->insert(out->end(), value.begin(), value.end());
}

std::vector<std::uint8_t> make_fixture() {
    std::vector<std::uint8_t> out;

    append_i32(&out, 2);
    append_u32(&out, 2u);
    append_string(&out, "one", 0x11u);
    append_f32(&out, 1.0f);
    append_f32(&out, 2.0f);
    append_u32(&out, 1u);
    append_f32(&out, 0.5f);
    append_u32(&out, 0u);
    append_u32(&out, 0u);
    append_u32(&out, 0u);
    assert(out.size() == 44u);
    append_string(&out, "two", 0x22u);
    append_f32(&out, 3.0f);
    append_f32(&out, 4.0f);
    append_u32(&out, 0u);
    assert(out.size() == 64u);

    append_u32(&out, 0u);
    assert(out.size() == 68u);

    append_u32(&out, 0u);
    append_u32(&out, 11u);
    append_u32(&out, 12u);
    append_u32(&out, 0u);
    append_u32(&out, 0u);
    append_u32(&out, 0u);
    assert(out.size() == 92u);

    append_u32(&out, 1u);
    append_string(&out, "two", 0x33u);
    append_string(&out, "", 0x44u);
    append_u32(&out, 21u);
    append_u32(&out, 22u);
    out.push_back(1u);
    out.push_back(0u);
    out.push_back(1u);
    append_u32(&out, 23u);
    append_u32(&out, 24u);
    append_u32(&out, 25u);
    append_string(&out, "", 0x55u);
    assert(out.size() == 137u);

    out.push_back(0xaau);
    out.push_back(0xbbu);
    return out;
}
}  // namespace

int main() {
    namespace model = nevergone::game_levels_model;
    const auto bytes = make_fixture();
    nevergone::hp_data::Reader reader(bytes);

    model::Model parsed;
    assert(model::parse(reader, &parsed));
    assert(parsed.end_offset == 137u);
    assert(parsed.end_offset < reader.size());
    assert(parsed.scenes.scenes.size() == 2u);
    assert(parsed.actions.actions.empty());
    assert(parsed.global.enemies.empty());
    assert(parsed.port_nodes.port_nodes.size() == 1u);
    assert(parsed.port_graph.nodes.size() == 1u);
    assert(parsed.port_graph.forward_link_count == 0u);
    assert(parsed.start_scene.port_node_index == 0u);
    assert(parsed.start_scene.scene_index == 1u);
    assert(parsed.start_scene.scene_guid == "two");
    assert(!parsed.start_scene.used_first_port_fallback);
    assert(parsed.navigation.current_port_node_index == 0u);
    assert(parsed.navigation.stored_event_port_type == 0u);

    std::vector<std::uint8_t> truncated = bytes;
    truncated.resize(136u);
    nevergone::hp_data::Reader truncated_reader(truncated);
    model::Model unchanged;
    unchanged.end_offset = 999u;
    assert(!model::parse(truncated_reader, &unchanged));
    assert(unchanged.end_offset == 999u);

    assert(!model::parse(reader, nullptr));
    return 0;
}
