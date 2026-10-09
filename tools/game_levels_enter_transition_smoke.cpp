#include <cassert>
#include <string>

#include "game_levels_enter_transition.h"

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
    probe.file_size = 45;
    probe.reader_size = 45;
    transition::on_enter_game_with_probe(probe);
    state = transition::snapshot();
    assert(state.boundary == Boundary::kVerifiedPrefixIncomplete);
    assert(state.verified_bytes == 0);

    probe.scene_prefix_readable = true;
    probe.first_scene_header_readable = true;
    probe.first_layer_header_readable = true;
    probe.first_object_prefix_readable = true;
    probe.first_object_prefix_bytes_consumed = 45;
    transition::on_enter_game_with_probe(probe);
    state = transition::snapshot();
    assert(state.boundary == Boundary::kFirstObjectPrefixVerified);
    assert(state.enter_callback_count == 5);
    assert(state.probe_attempt_count == 5);
    assert(state.file_size == 45);
    assert(state.reader_size == 45);
    assert(state.verified_bytes == 45);

    const std::string report = transition::status_report();
    assert(report.find("first-object-prefix-verified") != std::string::npos);
    assert(report.find("verified LoadGL_Scene bytes: 45") != std::string::npos);

    transition::reset();
    state = transition::snapshot();
    assert(state.boundary == Boundary::kIdle);
    assert(state.enter_callback_count == 0);
    return 0;
}
