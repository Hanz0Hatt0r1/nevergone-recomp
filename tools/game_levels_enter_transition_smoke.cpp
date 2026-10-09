#include <cassert>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include "game_levels_enter_transition.h"

namespace {
std::string make_temp_dir() {
    char pattern[] = "/tmp/nevergone-gamelevels-enter-XXXXXX";
    char* result = mkdtemp(pattern);
    assert(result != nullptr);
    return result;
}
void append_u32(std::vector<std::uint8_t>& out, std::uint32_t value) {
    for (unsigned shift = 0; shift < 32; shift += 8) out.push_back(static_cast<std::uint8_t>(value >> shift));
}
void append_i32(std::vector<std::uint8_t>& out, std::int32_t value) { append_u32(out, static_cast<std::uint32_t>(value)); }
void append_f32(std::vector<std::uint8_t>& out, float value) {
    std::uint32_t bits = 0; std::memcpy(&bits, &value, sizeof(bits)); append_u32(out, bits);
}
void append_core(std::vector<std::uint8_t>& out, std::int32_t type, const char* text, std::uint32_t length) {
    append_i32(out, type);
    append_u32(out, length);
    out.push_back(0xaau);
    out.insert(out.end(), text, text + length);
    for (int i = 0; i < 5; ++i) append_f32(out, static_cast<float>(i + 1));
    append_i32(out, 7);
    out.push_back(1u);
    out.push_back(0u);
}
std::vector<std::uint8_t> verified_layer_objects_fixture() {
    std::vector<std::uint8_t> out;
    append_i32(out, 2); append_u32(out, 1u);
    append_u32(out, 4u); out.push_back(0x7fu); out.insert(out.end(), {'h','e','r','o'});
    append_f32(out, 1.0f); append_f32(out, 2.0f); append_u32(out, 1u);
    append_f32(out, 0.75f); append_u32(out, 2u);
    assert(out.size() == 37u);

    append_core(out, 0, "a", 1u);
    assert(out.size() == 73u);

    append_core(out, 10, "node", 4u);
    append_u32(out, 11u); append_u32(out, 22u); append_u32(out, 4u);
    out.push_back(0x55u); out.insert(out.end(), {'t','a','i','l'}); append_i32(out, 0);
    append_i32(out, 33);
    append_f32(out, 10.0f); append_f32(out, 20.0f);
    append_f32(out, 30.0f); append_f32(out, 40.0f);
    assert(out.size() == 153u);
    return out;
}
void write_fixture(const std::filesystem::path& path) {
    const auto data = verified_layer_objects_fixture();
    std::ofstream output(path, std::ios::binary);
    output.write(reinterpret_cast<const char*>(data.data()), static_cast<std::streamsize>(data.size()));
    assert(output.good());
}
}  // namespace

int main() {
    namespace transition = nevergone::game_levels_enter_transition;
    using Boundary = transition::Boundary;
    using Probe = nevergone::game_levels_asset_probe::Snapshot;

    transition::reset();
    Probe probe;
    transition::on_enter_game_with_probe(probe);
    assert(transition::snapshot().boundary == Boundary::kFilesDirUnconfigured);
    probe.configured = true;
    transition::on_enter_game_with_probe(probe);
    assert(transition::snapshot().boundary == Boundary::kAssetMissing);
    probe.present = true;
    transition::on_enter_game_with_probe(probe);
    assert(transition::snapshot().boundary == Boundary::kAssetRejected);

    probe.regular_file = true; probe.within_size_limit = true; probe.loaded = true;
    probe.file_size = 153; probe.reader_size = 153;
    probe.first_object_record_readable = true;
    probe.first_object_record_bytes_consumed = 73;
    transition::on_enter_game_with_probe(probe);
    assert(transition::snapshot().boundary == Boundary::kVerifiedPrefixIncomplete);
    assert(transition::snapshot().verified_bytes == 0);

    probe.first_layer_objects_readable = true;
    probe.first_layer_objects_bytes_consumed = 153;
    transition::on_enter_game_with_probe(probe);
    auto state = transition::snapshot();
    assert(state.boundary == Boundary::kFirstLayerObjectsVerified);
    assert(state.verified_bytes == 153u);
    assert(transition::status_report().find("first-layer-objects-verified") != std::string::npos);

    transition::reset();
    const std::filesystem::path root(make_temp_dir());
    const auto scene = root / "assets" / "gamescene" / "gs_list" / "pvp_scene.glData";
    std::filesystem::create_directories(scene.parent_path());
    write_fixture(scene);
    transition::on_enter_game(root.string());
    state = transition::snapshot();
    assert(state.boundary == Boundary::kFirstLayerObjectsVerified);
    assert(state.verified_bytes == 153u);
    std::filesystem::remove_all(root);
    return 0;
}
