#include <cassert>
#include <string>

#include "app_delegate_state.h"

int main() {
    using nevergone::app_delegate_state::Phase;

    nevergone::app_delegate_state::reset();
    auto state = nevergone::app_delegate_state::snapshot();
    assert(state.phase == Phase::kCold);
    assert(!state.runtime_configured);

    nevergone::app_delegate_state::on_runtime_configured();
    state = nevergone::app_delegate_state::snapshot();
    assert(state.phase == Phase::kRuntimeConfigured);
    assert(state.runtime_configured);

    nevergone::app_delegate_state::on_surface_ready();
    state = nevergone::app_delegate_state::snapshot();
    assert(state.phase == Phase::kSurfaceReady);
    assert(state.surface_ready);
    assert(state.surface_generation == 1);

    // Asset reloads reuse the same recovered surface readiness boundary.
    nevergone::app_delegate_state::on_surface_ready();
    state = nevergone::app_delegate_state::snapshot();
    assert(state.phase == Phase::kSurfaceReady);
    assert(state.surface_generation == 1);

    nevergone::app_delegate_state::on_app_resume();
    state = nevergone::app_delegate_state::snapshot();
    assert(state.phase == Phase::kForegroundReady);
    assert(state.resumed);
    assert(state.resume_count == 1);

    nevergone::app_delegate_state::on_scene_sequence_reset(7);
    state = nevergone::app_delegate_state::snapshot();
    assert(state.phase == Phase::kForegroundReady);
    assert(state.scene_generation == 7);
    assert(!state.scene_sequence_started);

    nevergone::app_delegate_state::on_scene_sequence_begin(8, 35);
    state = nevergone::app_delegate_state::snapshot();
    assert(state.phase == Phase::kSplashRunning);
    assert(state.scene_sequence_started);
    assert(state.scene_generation == 8);
    assert(state.scene_start_tick == 35);

    nevergone::app_delegate_state::on_frame(100, false);
    state = nevergone::app_delegate_state::snapshot();
    assert(state.phase == Phase::kSplashRunning);
    assert(state.last_tick == 100);

    nevergone::app_delegate_state::on_app_pause();
    state = nevergone::app_delegate_state::snapshot();
    assert(state.phase == Phase::kBackground);
    assert(!state.resumed);
    assert(state.pause_count == 1);

    // A GL/resource reload while paused must not make the app foreground-ready.
    nevergone::app_delegate_state::on_surface_ready();
    state = nevergone::app_delegate_state::snapshot();
    assert(state.phase == Phase::kBackground);
    assert(state.surface_generation == 1);

    nevergone::app_delegate_state::on_frame(140, true);
    state = nevergone::app_delegate_state::snapshot();
    assert(state.phase == Phase::kBackground);
    assert(state.scene_sequence_complete);

    nevergone::app_delegate_state::on_app_resume();
    state = nevergone::app_delegate_state::snapshot();
    assert(state.phase == Phase::kInitialUiReady);
    assert(state.resume_count == 2);

    const std::string report = nevergone::app_delegate_state::status_report();
    assert(report.find("phase: initial-ui-ready") != std::string::npos);
    assert(report.find("generation 8") != std::string::npos);
    return 0;
}
