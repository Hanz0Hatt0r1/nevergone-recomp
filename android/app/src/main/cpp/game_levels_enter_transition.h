#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

#include "game_levels_asset_probe.h"

namespace nevergone::game_levels_enter_transition {

enum class Boundary {
    kIdle = 0,
    kFilesDirUnconfigured,
    kAssetMissing,
    kAssetRejected,
    kVerifiedPrefixIncomplete,
    kSceneSectionVerified,
    kActionsSectionVerified,
    kGlobalSectionVerified,
    kPortNodeSectionVerified,
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
void on_enter_game(const std::string& files_dir);
void on_enter_game_with_probe(const game_levels_asset_probe::Snapshot& probe);
Snapshot snapshot();
const char* boundary_name(Boundary boundary);
std::string status_report();

}  // namespace nevergone::game_levels_enter_transition
