#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

namespace nevergone::tutorial_scene_bootstrap {

enum class Stage : int {
    kIdle = 0,
    kResolvingScene,
    kFilesDirUnconfigured,
    kSceneMissing,
    kSceneRejected,
    kRuntimeLoadFailed,
    kRuntimeQueueUnready,
    kPrepared,
    kWaitingForOriginalSwitchDelay,
    kWaitingForSceneFrame,
    kRuntimeChanged,
    kSessionTransitionRejected,
    kRouteTransitionRejected,
    kRouteChanged,
    kComplete,
};

enum class Resolution : int {
    kNone = 0,
    kOriginalSearchPaths,
};

struct Snapshot {
    Stage stage = Stage::kIdle;
    Resolution resolution = Resolution::kNone;
    std::uint64_t fresh_role_generation = 0;
    std::uint64_t prepare_attempt_count = 0;
    std::uint64_t loading_start_count = 0;
    std::uint64_t loading_presentation_count = 0;
    std::uint64_t loading_start_game_tick = 0;
    std::uint64_t current_game_tick = 0;
    std::uint64_t switch_callback_due_count = 0;
    std::uint64_t scene_draw_observation_count = 0;
    std::uint64_t completion_count = 0;
    std::string scene_relative_path;
    std::uint64_t scene_file_size = 0;
    std::size_t verified_bytes = 0;
    std::uint64_t runtime_load_attempt_count = 0;
    std::string scene_guid;
    std::size_t render_sprite_count = 0;
};

// Original GameSceneUI::showLoadingLayer() creates CCDelayTime(0.5f) before
// invoking GameScene::switchNextScene(). The preservation runtime advances on
// a fixed 35 Hz clock, so the callback becomes eligible on the first complete
// logical tick at or after 0.5 seconds.
constexpr double kOriginalSwitchDelaySeconds = 0.5;
constexpr std::uint64_t kOriginalSwitchDelayTicks = 18u;

// GameSceneUI::hideLoadingLayer(0.2f) starts after LessonLayer is created.
// At the recovered fixed 35 Hz game clock, 0.2 seconds is exactly seven
// logical ticks; keep the integer boundary explicit so floating-point rounding
// cannot extend the fade by an extra frame.
constexpr double kOriginalHideFadeSeconds = 0.2;
constexpr std::uint64_t kOriginalHideFadeTicks = 7u;

void reset();

// Reconstruct ManagementLayer::GoToGameLayer's fresh-role branch: resolve the
// shipped bare filename cp_lesson.glData using the recovered Cocos search-path
// order, then parse/load it before LoadingLayer is entered. Returns true only
// when the exact retained GameLevels runtime has a ready render queue.
bool prepare(const std::string& files_dir);

// Arm the recovered LoadingLayer -> 0.5 s -> switchNextScene boundary after the
// caller has changed offline_startup_flow to tutorial-loading.
bool begin_loading();

// Called once per render frame after game_clock::advance(). It makes the
// switchNextScene callback eligible after the recovered 0.5 second delay and
// only after at least one actual loading presentation has been observed.
void tick();

// Record one frame in which the reconstructed loading layer actually owned the
// surface.
void on_loading_frame_presented();

// Feed the result of the project-owned GameScene renderer. Once the original
// switch delay is due, a successful frame from the same retained cp_lesson
// runtime provides the clean-room equivalent of switchNextScene/initGameScene
// being ready for the first tick that shows LessonLayer.
void observe_scene_draw(bool drawn);

Snapshot snapshot();
const char* stage_name(Stage stage);
const char* resolution_name(Resolution resolution);
std::string status_report();

}  // namespace nevergone::tutorial_scene_bootstrap
