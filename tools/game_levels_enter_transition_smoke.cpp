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
std::vector<std::uint8_t> verified_scene_section_fixture() {
    std::vector<std::uint8_t> out;
    append_i32(out, 2); append_u32(out, 2u);

    append_u32(out, 3u); out.push_back(0x11u); out.insert(out.end(), {'o','n','e'});
    append_f32(out, 1.0f); append_f32(out, 2.0f); append_u32(out, 1u);
    append_f32(out, 0.5f); append_u32(out, 0u);
    append_u32(out, 0u); append_u32(out, 0u);
    assert(out.size() == 44u);

    append_u32(out, 3u); out.push_back(0x22u); out.insert(out.end(), {'t','w','o'});
    append_f32(out, 3.0f); append_f32(out, 4.0f); append_u32(out, 0u);
    assert(out.size() == 64u);
    return out;
}
std::vector<std::uint8_t> verified_actions_section_fixture() {
    auto out = verified_scene_section_fixture();
    append_u32(out, 0u);
    assert(out.size() == 68u);
    return out;
}
void write_fixture(const std::filesystem::path& path) {
    const auto data = verified_actions_section_fixture();
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
    probe.file_size = 68; probe.reader_size = 68;
    probe.first_scene_layers_readable = true;
    probe.first_scene_layers_bytes_consumed = 44;
    transition::on_enter_game_with_probe(probe);
    assert(transition::snapshot().boundary == Boundary::kVerifiedPrefixIncomplete);
    assert(transition::snapshot().verified_bytes == 0);

    probe.scene_section_readable = true;
    probe.scene_section_bytes_consumed = 64;
    transition::on_enter_game_with_probe(probe);
    auto state = transition::snapshot();
    assert(state.boundary == Boundary::kSceneSectionVerified);
    assert(state.verified_bytes == 64u);
    assert(transition::status_report().find("scene-section-verified") != std::string::npos);

    probe.actions_section_readable = true;
    probe.actions_section_bytes_consumed = 68;
    transition::on_enter_game_with_probe(probe);
    state = transition::snapshot();
    assert(state.boundary == Boundary::kActionsSectionVerified);
    assert(state.verified_bytes == 68u);
    assert(transition::status_report().find("actions-section-verified") != std::string::npos);

    transition::reset();
    const std::filesystem::path root(make_temp_dir());
    const auto scene = root / "assets" / "gamescene" / "gs_list" / "pvp_scene.glData";
    std::filesystem::create_directories(scene.parent_path());
    write_fixture(scene);
    transition::on_enter_game(root.string());
    state = transition::snapshot();
    assert(state.boundary == Boundary::kActionsSectionVerified);
    assert(state.verified_bytes == 68u);
    std::filesystem::remove_all(root);
    return 0;
}
