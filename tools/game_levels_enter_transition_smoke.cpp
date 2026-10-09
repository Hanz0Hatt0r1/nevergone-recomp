#include <cassert>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>

#include "game_levels_enter_transition.h"

namespace {

std::string make_temp_dir() {
    char pattern[] = "/tmp/nevergone-gamelevels-enter-XXXXXX";
    char* result = mkdtemp(pattern);
    assert(result != nullptr);
    return result;
}

void write_verified_header_fixture(const std::filesystem::path& path) {
    std::ofstream output(path, std::ios::binary);
    assert(output);
    const std::string data(
        "\x01\x00\x00\x00"  // unresolved signed header field
        "\x01\x00\x00\x00"  // scene_count
        "\x04\x00\x00\x00"  // first scene string length
        "\x7fhero"
        "\x00\x00\x80\x3f"  // 1.0f
        "\x00\x00\x00\x40"  // 2.0f
        "\x01\x00\x00\x00"  // layer_count
        "\x00\x00\x40\x3f"  // layer float
        "\x01\x00\x00\x00"  // object_count
        "\xfd\xff\xff\xff"  // first object int32
        "\x03\x00\x00\x00"  // object string length
        "\xaaobj"
        "\x00\x00\x20\x41"  // 10.0f
        "\x00\x00\xa0\x41"  // 20.0f
        "\x00\x00\xf0\x41"  // 30.0f
        "\x00\x00\x20\x42"  // 40.0f
        "\x00\x00\x48\x42"  // 50.0f
        "\xf9\xff\xff\xff"  // second object int32
        "\x01\x00",           // two bools
        75);
    output.write(data.data(), static_cast<std::streamsize>(data.size()));
    assert(output.good());
}

}  // namespace

int main() {
    namespace transition = nevergone::game_levels_enter_transition;
    using Boundary = transition::Boundary;
    using Probe = nevergone::game_levels_asset_probe::Snapshot;

    transition::reset();
    auto state = transition::snapshot();
    assert(state.boundary == Boundary::kIdle);
    assert(state.enter_callback_count == 0);
    assert(state.probe_attempt_count == 0);

    Probe probe;
    transition::on_enter_game_with_probe(probe);
    state = transition::snapshot();
    assert(state.boundary == Boundary::kFilesDirUnconfigured);
    assert(state.enter_callback_count == 1);
    assert(state.probe_attempt_count == 1);

    probe.configured = true;
    transition::on_enter_game_with_probe(probe);
    state = transition::snapshot();
    assert(state.boundary == Boundary::kAssetMissing);

    probe.present = true;
    transition::on_enter_game_with_probe(probe);
    state = transition::snapshot();
    assert(state.boundary == Boundary::kAssetRejected);

    probe.regular_file = true;
    probe.within_size_limit = true;
    probe.loaded = true;
    probe.file_size = 75;
    probe.reader_size = 75;
    transition::on_enter_game_with_probe(probe);
    state = transition::snapshot();
    assert(state.boundary == Boundary::kVerifiedPrefixIncomplete);
    assert(state.verified_bytes == 0);

    // A readable old 45-byte object prefix is no longer the strongest verified
    // boundary; the transition must wait for the complete non-versioned header.
    probe.scene_prefix_readable = true;
    probe.first_scene_header_readable = true;
    probe.first_layer_header_readable = true;
    probe.first_object_prefix_readable = true;
    probe.first_object_prefix_bytes_consumed = 45;
    transition::on_enter_game_with_probe(probe);
    state = transition::snapshot();
    assert(state.boundary == Boundary::kVerifiedPrefixIncomplete);
    assert(state.verified_bytes == 0);

    probe.first_object_header_readable = true;
    probe.first_object_header_bytes_consumed = 75;
    transition::on_enter_game_with_probe(probe);
    state = transition::snapshot();
    assert(state.boundary == Boundary::kFirstObjectHeaderVerified);
    assert(state.enter_callback_count == 6);
    assert(state.probe_attempt_count == 6);
    assert(state.file_size == 75);
    assert(state.reader_size == 75);
    assert(state.verified_bytes == 75);

    const std::string report = transition::status_report();
    assert(report.find("first-object-header-verified") != std::string::npos);
    assert(report.find("verified LoadGL_Scene bytes: 75") != std::string::npos);

    // Production path: cpp_OnEnterGame supplies the app files directory. Make
    // sure the transition probes exactly the recovered imported scene path and
    // reaches only the same verified 75-byte fixture boundary.
    transition::reset();
    const std::filesystem::path root(make_temp_dir());
    const std::filesystem::path scene =
        root / "assets" / "gamescene" / "gs_list" / "pvp_scene.glData";
    std::filesystem::create_directories(scene.parent_path());
    write_verified_header_fixture(scene);
    transition::on_enter_game(root.string());
    state = transition::snapshot();
    assert(state.boundary == Boundary::kFirstObjectHeaderVerified);
    assert(state.enter_callback_count == 1);
    assert(state.probe_attempt_count == 1);
    assert(state.verified_bytes == 75);
    std::filesystem::remove_all(root);

    transition::reset();
    state = transition::snapshot();
    assert(state.boundary == Boundary::kIdle);
    assert(state.enter_callback_count == 0);
    return 0;
}
