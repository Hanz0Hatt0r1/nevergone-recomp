#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

namespace nevergone::tutorial_scene_bootstrap {

enum class Stage : int {
    kIdle = 0,
    kWaitingForLoadingPresentation,
    kResolvingScene,
    kFilesDirUnconfigured,
    kAssetRootUnavailable,
    kSceneMissing,
    kSceneAmbiguous,
    kSearchLimitReached,
    kSceneRejected,
    kRuntimeLoadFailed,
    kRuntimeQueueUnready,
    kWaitingForSceneFrame,
    kRuntimeChanged,
    kSessionTransitionRejected,
    kRouteTransitionRejected,
    kRouteChanged,
    kComplete,
};

enum class Resolution : int {
    kNone = 0,
    kExactPath,
    kRecursiveBasename,
};

struct Snapshot {
    Stage stage = Stage::kIdle;
    Resolution resolution = Resolution::kNone;
    std::uint64_t fresh_role_generation = 0;
    std::uint64_t bootstrap_attempt_count = 0;
    std::uint64_t loading_presentation_count = 0;
    std::uint64_t scene_draw_observation_count = 0;
    std::uint64_t completion_count = 0;
    std::size_t searched_entry_count = 0;
    std::string scene_relative_path;
    std::uint64_t scene_file_size = 0;
    std::size_t verified_bytes = 0;
    std::uint64_t runtime_load_attempt_count = 0;
    std::string scene_guid;
    std::size_t render_sprite_count = 0;
};

void reset();

// Called once per render frame before the project-owned GameScene renderer.
// The bootstrap does no work outside the explicit fresh-role tutorial-loading
// route. It waits until that loading presentation has actually owned at least
// one frame, then resolves and parses the imported cp_lesson.glData without an
// invented timer.
void tick(const std::string& files_dir);

// Record one frame in which the reconstructed loading layer actually owned the
// surface. The caller should invoke this only after drawing that layer while
// the route was still tutorial-loading.
void on_loading_frame_presented();

// Feed the result of the project-owned GameScene renderer. A successful frame
// from the retained cp_lesson runtime is the concrete completion evidence that
// allows Loading -> LessonOverlay and tutorial-loading -> game-levels.
void observe_scene_draw(bool drawn);

Snapshot snapshot();
const char* stage_name(Stage stage);
const char* resolution_name(Resolution resolution);
std::string status_report();

}  // namespace nevergone::tutorial_scene_bootstrap
