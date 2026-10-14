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
    kRuntimeModelReady,
    kRuntimeSceneInstanceReady,
    kRuntimeSceneConstructionPlanReady,
    kRuntimeRenderQueueReady,
};

struct Snapshot {
    Boundary boundary = Boundary::kIdle;
    std::uint64_t enter_callback_count = 0;
    std::uint64_t local_enter_count = 0;
    std::uint64_t probe_attempt_count = 0;
    std::uint64_t file_size = 0;
    std::size_t reader_size = 0;
    std::size_t verified_bytes = 0;
};

void reset();

// Callback-driven recovered online boundary.
void on_enter_game(const std::string& files_dir);

// Project-owned serverless preservation boundary for the known pvp_scene path.
// It loads the same verified GameLevels model but does not increment/claim an
// original enter callback.
void on_local_enter_game(const std::string& files_dir);

// Project-owned local transition for an already resolved original GameLevels
// filename. This is used by the recovered fresh-role tutorial path where the
// shipped ManagementLayer passes the bare cp_lesson.glData through the normal
// Cocos search-path resolver before GameScene::switchScene().
void on_local_enter_game_file(const std::string& resolved_path);

// Probe-injection helper used by callback-path host tests.
void on_enter_game_with_probe(const game_levels_asset_probe::Snapshot& probe);
Snapshot snapshot();
const char* boundary_name(Boundary boundary);
std::string status_report();

}  // namespace nevergone::game_levels_enter_transition
