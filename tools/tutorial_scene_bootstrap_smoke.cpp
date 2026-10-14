#include <cassert>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include "game_levels_runtime_state.h"
#include "offline_startup_flow.h"
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

std::vector<std::uint8_t> fixture() {
    std::vector<std::uint8_t> out;
    append_i32(&out, 2);
    append_u32(&out, 2u);

    append_string(&out, "lesson_a", 0x11u);
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
    append_port(&out, "lesson_a", true, "lesson_b");
    append_port(&out, "lesson_b", false, "");
    return out;
}

void write_fixture(const std::filesystem::path& path) {
    std::filesystem::create_directories(path.parent_path());
    const auto bytes = fixture();
    std::ofstream output(path, std::ios::binary);
    output.write(
        reinterpret_cast<const char*>(bytes.data()),
        static_cast<std::streamsize>(bytes.size()));
    assert(output.good());
}

void enter_loading_route() {
    namespace flow = nevergone::offline_startup_flow;
    namespace bootstrap = nevergone::tutorial_scene_bootstrap;
    namespace session = nevergone::tutorial_session_state;

    bootstrap::reset();
    session::reset();
    flow::reset();
    flow::on_auto_login_compat_success(1u);
    flow::set_standalone_hero_presence(false);
    assert(flow::snapshot().route == flow::Route::kOpeningDialogue);
    assert(flow::on_fresh_role_local_enter());
    assert(flow::snapshot().route == flow::Route::kTutorialLoading);
    assert(flow::snapshot().fresh_role_local_enter_count == 1u);
}

}  // namespace

int main() {
    namespace bootstrap = nevergone::tutorial_scene_bootstrap;
    namespace flow = nevergone::offline_startup_flow;
    namespace runtime = nevergone::game_levels_runtime_state;
    namespace session = nevergone::tutorial_session_state;

    const auto root =
        std::filesystem::temp_directory_path() / "nevergone-tutorial-bootstrap-smoke";
    std::filesystem::remove_all(root);
    std::filesystem::create_directories(root / "assets");

    // The evidence-backed basename may live outside the conventional
    // gamescene/gs_list directory in an imported tree. Prove the bounded
    // recursive resolver and require one actual loading presentation before
    // the GameLevels parse is attempted.
    const auto recursive_scene =
        root / "assets" / "tutorial" / "scene" / "cp_lesson.glData";
    write_fixture(recursive_scene);
    enter_loading_route();

    bootstrap::tick(root.string());
    auto state = bootstrap::snapshot();
    assert(state.stage == bootstrap::Stage::kWaitingForLoadingPresentation);
    assert(state.bootstrap_attempt_count == 0u);
    assert(session::snapshot().phase == session::Phase::kLoading);
    assert(flow::snapshot().route == flow::Route::kTutorialLoading);

    bootstrap::tick(root.string());
    assert(bootstrap::snapshot().bootstrap_attempt_count == 0u);

    bootstrap::on_loading_frame_presented();
    bootstrap::tick(root.string());
    state = bootstrap::snapshot();
    assert(state.stage == bootstrap::Stage::kWaitingForSceneFrame);
    assert(state.resolution == bootstrap::Resolution::kRecursiveBasename);
    assert(state.scene_relative_path == "tutorial/scene/cp_lesson.glData");
    assert(state.bootstrap_attempt_count == 1u);
    assert(state.loading_presentation_count == 1u);
    assert(state.verified_bytes == fixture().size());
    assert(state.runtime_load_attempt_count != 0u);
    assert(state.scene_guid == "lesson_a");
    assert(runtime::snapshot().status == runtime::LoadStatus::kReady);
    assert(runtime::snapshot().current_scene_render_queue_ready);
    assert(flow::snapshot().route == flow::Route::kTutorialLoading);
    assert(session::snapshot().phase == session::Phase::kLoading);

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
    assert(bootstrap::status_report().find("state: complete") != std::string::npos);

    // Missing cp_lesson remains fail-closed on the loading presentation.
    std::filesystem::remove_all(root / "assets");
    std::filesystem::create_directories(root / "assets");
    enter_loading_route();
    bootstrap::tick(root.string());
    bootstrap::on_loading_frame_presented();
    bootstrap::tick(root.string());
    state = bootstrap::snapshot();
    assert(state.stage == bootstrap::Stage::kSceneMissing);
    assert(flow::snapshot().route == flow::Route::kTutorialLoading);
    assert(session::snapshot().phase == session::Phase::kLoading);

    // Two fallback basename matches are ambiguous and must not be guessed.
    write_fixture(root / "assets" / "one" / "cp_lesson.glData");
    write_fixture(root / "assets" / "two" / "cp_lesson.glData");
    enter_loading_route();
    bootstrap::tick(root.string());
    bootstrap::on_loading_frame_presented();
    bootstrap::tick(root.string());
    state = bootstrap::snapshot();
    assert(state.stage == bootstrap::Stage::kSceneAmbiguous);
    assert(flow::snapshot().route == flow::Route::kTutorialLoading);

    // If the conventional GameLevels path is present, it is authoritative and
    // deterministic even when unrelated duplicate basenames exist elsewhere.
    write_fixture(root / "assets" / "gamescene" / "gs_list" / "cp_lesson.glData");
    enter_loading_route();
    bootstrap::tick(root.string());
    bootstrap::on_loading_frame_presented();
    bootstrap::tick(root.string());
    state = bootstrap::snapshot();
    assert(state.stage == bootstrap::Stage::kWaitingForSceneFrame);
    assert(state.resolution == bootstrap::Resolution::kExactPath);
    assert(state.scene_relative_path == "gamescene/gs_list/cp_lesson.glData");

    std::filesystem::remove_all(root);
    bootstrap::reset();
    session::reset();
    flow::reset();
    runtime::reset();
    return 0;
}
