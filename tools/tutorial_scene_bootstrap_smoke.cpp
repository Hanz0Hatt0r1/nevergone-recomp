#include <cassert>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>
#include <thread>
#include <vector>

#include "game_clock.h"
#include "game_levels_enter_transition.h"
#include "game_levels_runtime_state.h"
#include "offline_startup_flow.h"
#include "startup_contract.h"
#include "tutorial_scene_bootstrap.h"
#include "tutorial_session_state.h"

namespace {

void append_u32(std::vector<std::uint8_t>* out, std::uint32_t value) {
    for (unsigned shift = 0; shift < 32; shift += 8) {
        out->push_back(static_cast<std::uint8_t>(value >> shift));
    }
}

void append_i32(std::vector<std::uint8_t>* out, std::int32_t value) {
    append_u32(out, static_cast<std::uint32_t>(value));
}

void append_f32(std::vector<std::uint8_t>* out, float value) {
    std::uint32_t bits = 0;
    std::memcpy(&bits, &value, sizeof(bits));
    append_u32(out, bits);
}

void append_string(
        std::vector<std::uint8_t>* out,
        const std::string& value,
        std::uint8_t gap) {
    append_u32(out, static_cast<std::uint32_t>(value.size()));
    out->push_back(gap);
    out->insert(out->end(), value.begin(), value.end());
}

void append_port(
        std::vector<std::uint8_t>* out,
        const std::string& first,
        bool start,
        const std::string& third) {
    append_string(out, first, 0x31u);
    append_string(out, "", 0x32u);
    append_u32(out, 1u);
    append_u32(out, 2u);
    out->push_back(start ? 1u : 0u);
    out->push_back(0u);
    out->push_back(0u);
    append_u32(out, 3u);
    append_u32(out, 4u);
    append_u32(out, 5u);
    append_string(out, third, 0x33u);
}

std::vector<std::uint8_t> fixture(const std::string& first_guid = "lesson_a") {
    std::vector<std::uint8_t> out;
    append_i32(&out, 2);
    append_u32(&out, 2u);

    append_string(&out, first_guid, 0x11u);
    append_f32(&out, 1.0f);
    append_f32(&out, 2.0f);
    append_u32(&out, 1u);
    append_f32(&out, 0.5f);
    append_u32(&out, 0u);
    append_u32(&out, 0u);
    append_u32(&out, 0u);

    append_string(&out, "lesson_b", 0x12u);
    append_f32(&out, 3.0f);
    append_f32(&out, 4.0f);
    append_u32(&out, 0u);

    append_u32(&out, 0u);

    append_u32(&out, 2u);
    append_string(&out, "cp_lesson_default.plist", 0x41u);
    append_string(&out, "cp_lesson_effect.plist", 0x42u);
    append_u32(&out, 11u);
    append_u32(&out, 12u);
    append_u32(&out, 0u);
    append_u32(&out, 0u);
    append_u32(&out, 0u);

    append_u32(&out, 2u);
    append_port(&out, first_guid, true, "lesson_b");
    append_port(&out, "lesson_b", false, "");
    return out;
}

void write_fixture(
        const std::filesystem::path& path,
        const std::string& first_guid = "lesson_a") {
    std::filesystem::create_directories(path.parent_path());
    const auto bytes = fixture(first_guid);
    std::ofstream output(path, std::ios::binary);
    output.write(
        reinterpret_cast<const char*>(bytes.data()),
        static_cast<std::streamsize>(bytes.size()));
    assert(output.good());
}

void reset_opening_route() {
    namespace flow = nevergone::offline_startup_flow;
    namespace bootstrap = nevergone::tutorial_scene_bootstrap;
    namespace session = nevergone::tutorial_session_state;

    bootstrap::reset();
    session::reset();
    nevergone::game_levels_enter_transition::reset();
    flow::reset();
    flow::on_auto_login_compat_success(1u);
    flow::set_standalone_hero_presence(false);
    assert(flow::snapshot().route == flow::Route::kOpeningDialogue);
}

void advance_past_original_switch_delay() {
    using namespace std::chrono_literals;
    // The fixed game clock clamps each frame and catch-up batch. Advance in
    // normal frame-sized pieces so the test proves the same >=0.5 s logical
    // boundary used on Android rather than depending on one wall-clock jump.
    for (int index = 0; index < 8; ++index) {
        std::this_thread::sleep_for(80ms);
        (void)nevergone::game_clock::advance();
    }
    assert(nevergone::game_clock::tick_count() >=
           nevergone::tutorial_scene_bootstrap::kOriginalSwitchDelayTicks);
}

}  // namespace

int main() {
    namespace bootstrap = nevergone::tutorial_scene_bootstrap;
    namespace enter = nevergone::game_levels_enter_transition;
    namespace flow = nevergone::offline_startup_flow;
    namespace runtime = nevergone::game_levels_runtime_state;
    namespace session = nevergone::tutorial_session_state;

    const auto root =
        std::filesystem::temp_directory_path() / "nevergone-tutorial-bootstrap-smoke";
    std::filesystem::remove_all(root);
    std::filesystem::create_directories(root / "assets");
    nevergone::startup::configure({root.string(), "device", "test", "android"});

    // The shipped fresh-role call site passes the bare name cp_lesson.glData.
    // The recovered AddAllSearchPath ordering must therefore find the normal
    // gamescene/gs_list copy before LoadingLayer begins.
    const auto scene =
        root / "assets" / "gamescene" / "gs_list" / "cp_lesson.glData";
    write_fixture(scene);
    reset_opening_route();

    assert(bootstrap::prepare(root.string()));
    auto state = bootstrap::snapshot();
    assert(state.stage == bootstrap::Stage::kPrepared);
    assert(state.resolution == bootstrap::Resolution::kOriginalSearchPaths);
    assert(state.scene_relative_path == "gamescene/gs_list/cp_lesson.glData");
    assert(state.prepare_attempt_count == 1u);
    assert(state.verified_bytes == fixture().size());
    assert(state.runtime_load_attempt_count != 0u);
    assert(state.scene_guid == "lesson_a");
    assert(runtime::snapshot().status == runtime::LoadStatus::kReady);
    assert(runtime::snapshot().current_scene_render_queue_ready);
    assert(enter::snapshot().boundary == enter::Boundary::kRuntimeRenderQueueReady);
    assert(enter::snapshot().local_enter_count == 1u);
    assert(flow::snapshot().route == flow::Route::kOpeningDialogue);

    // switchScene follows LoadGameLevelsWithFile in the original call site.
    assert(flow::on_fresh_role_local_enter());
    assert(flow::snapshot().route == flow::Route::kTutorialLoading);

    nevergone::game_clock::reset();
    nevergone::game_clock::resume();
    assert(bootstrap::begin_loading());
    state = bootstrap::snapshot();
    assert(state.stage == bootstrap::Stage::kWaitingForOriginalSwitchDelay);
    assert(state.loading_start_count == 1u);
    assert(session::snapshot().phase == session::Phase::kLoading);

    // A valid scene frame before the recovered CCDelayTime(0.5f) callback must
    // not advance the route.
    bootstrap::on_loading_frame_presented();
    bootstrap::observe_scene_draw(true);
    bootstrap::tick();
    assert(bootstrap::snapshot().stage == bootstrap::Stage::kWaitingForOriginalSwitchDelay);
    assert(bootstrap::snapshot().scene_draw_observation_count == 0u);
    assert(flow::snapshot().route == flow::Route::kTutorialLoading);

    advance_past_original_switch_delay();
    bootstrap::tick();
    state = bootstrap::snapshot();
    assert(state.stage == bootstrap::Stage::kWaitingForSceneFrame);
    assert(state.switch_callback_due_count == 1u);
    assert(state.loading_presentation_count == 1u);

    bootstrap::observe_scene_draw(false);
    assert(bootstrap::snapshot().stage == bootstrap::Stage::kWaitingForSceneFrame);
    assert(bootstrap::snapshot().scene_draw_observation_count == 1u);
    assert(flow::snapshot().route == flow::Route::kTutorialLoading);

    bootstrap::observe_scene_draw(true);
    state = bootstrap::snapshot();
    assert(state.stage == bootstrap::Stage::kComplete);
    assert(state.scene_draw_observation_count == 2u);
    assert(state.completion_count == 1u);
    assert(flow::snapshot().route == flow::Route::kGameLevels);
    assert(flow::snapshot().tutorial_loading_complete_count == 1u);
    assert(session::snapshot().phase == session::Phase::kLessonOverlay);
    assert(session::snapshot().transition_count == 1u);
    assert(bootstrap::status_report().find("original switch delay seconds: 0.5") !=
           std::string::npos);
    assert(bootstrap::status_report().find("original hide fade seconds: 0.2") !=
           std::string::npos);

    // Missing cp_lesson fails before the loading route is entered, matching the
    // original LoadGameLevelsWithFile -> switchScene ordering.
    std::filesystem::remove_all(root / "assets");
    std::filesystem::create_directories(root / "assets");
    reset_opening_route();
    assert(!bootstrap::prepare(root.string()));
    state = bootstrap::snapshot();
    assert(state.stage == bootstrap::Stage::kSceneMissing);
    assert(flow::snapshot().route == flow::Route::kOpeningDialogue);

    // Search-path order is authoritative: the assets root precedes the 59
    // child directories. A direct cp_lesson therefore wins over gs_list, just
    // as CCFileUtils fullPathForFilename would in the shipped runtime.
    write_fixture(
        root / "assets" / "gamescene" / "gs_list" / "cp_lesson.glData",
        "search_path_late");
    write_fixture(root / "assets" / "cp_lesson.glData", "search_path_first");
    reset_opening_route();
    assert(bootstrap::prepare(root.string()));
    state = bootstrap::snapshot();
    assert(state.scene_relative_path == "cp_lesson.glData");
    assert(state.scene_guid == "search_path_first");

    std::filesystem::remove_all(root);
    bootstrap::reset();
    session::reset();
    flow::reset();
    runtime::reset();
    enter::reset();
    nevergone::game_clock::pause();
    return 0;
}
