#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

#include "game_levels_asset_probe.h"

namespace nevergone::game_levels_enter_transition {

// This state deliberately stops at the strongest currently verified
// LoadGL_Scene boundary. "Ready" here means ready for the next reconstruction
// step, not that a GameScene has been instantiated or rendered.
enum class Boundary {
    kIdle = 0,
    kFilesDirUnconfigured,
    kAssetMissing,
    kAssetRejected,
    kVerifiedPrefixIncomplete,
    kFirstObjectPrefixVerified,
};

struct Snapshot {
    Boundary boundary = Boundary::kIdle;
    std::uint64_t enter_callback_count = 0;
    std::uint64_t probe_attempt_count = 0;
    std::uint64_t file_size = 0;
    std::size_t reader_size = 0;
    std::size_t verified_bytes = 0;
};

void reset();

// Production entry point called when the reconstructed Management route
// receives cpp_OnEnterGame. It probes only the already recovered user-owned
// pvp_scene.glData path and never parses beyond game_levels_asset_probe.
void on_enter_game(const std::string& files_dir);

// Injectable semantic boundary for host tests. The supplied probe is treated
// exactly like the result of probe_pvp_scene().
void on_enter_game_with_probe(const game_levels_asset_probe::Snapshot& probe);

Snapshot snapshot();
const char* boundary_name(Boundary boundary);
std::string status_report();

}  // namespace nevergone::game_levels_enter_transition
