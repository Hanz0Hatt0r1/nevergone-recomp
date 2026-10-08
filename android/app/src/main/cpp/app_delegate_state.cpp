#include "app_delegate_state.h"

#include <mutex>
#include <sstream>

namespace nevergone::app_delegate_state {
namespace {

std::mutex g_mutex;
Snapshot g_state;

void update_foreground_phase_locked() {
    if (!g_state.resumed && (g_state.pause_count != 0 || g_state.resume_count != 0)) {
        g_state.phase = Phase::kBackground;
    } else if (g_state.scene_sequence_complete) {
        g_state.phase = Phase::kInitialUiReady;
    } else if (g_state.scene_sequence_started) {
        g_state.phase = Phase::kSplashRunning;
    } else if (g_state.surface_ready) {
        g_state.phase = g_state.resumed ? Phase::kForegroundReady : Phase::kSurfaceReady;
    } else if (g_state.runtime_configured) {
        g_state.phase = Phase::kRuntimeConfigured;
    } else {
        g_state.phase = Phase::kCold;
    }
}

}  // namespace

void reset() {
    std::lock_guard<std::mutex> lock(g_mutex);
    g_state = Snapshot{};
}

void on_runtime_configured() {
    std::lock_guard<std::mutex> lock(g_mutex);
    g_state.runtime_configured = true;
    if (g_state.phase == Phase::kCold) g_state.phase = Phase::kRuntimeConfigured;
}

void on_surface_ready() {
    std::lock_guard<std::mutex> lock(g_mutex);
    if (!g_state.surface_ready) {
        g_state.surface_ready = true;
        ++g_state.surface_generation;
    }
    update_foreground_phase_locked();
}

void on_app_pause() {
    std::lock_guard<std::mutex> lock(g_mutex);
    g_state.resumed = false;
    ++g_state.pause_count;
    g_state.phase = Phase::kBackground;
}

void on_app_resume() {
    std::lock_guard<std::mutex> lock(g_mutex);
    g_state.resumed = true;
    ++g_state.resume_count;
    update_foreground_phase_locked();
}

void on_scene_sequence_reset(std::uint64_t scene_generation) {
    std::lock_guard<std::mutex> lock(g_mutex);
    g_state.scene_sequence_started = false;
    g_state.scene_sequence_complete = false;
    g_state.scene_generation = scene_generation;
    g_state.scene_start_tick = 0;
    g_state.last_tick = 0;
    update_foreground_phase_locked();
}

void on_scene_sequence_begin(std::uint64_t scene_generation, std::uint64_t start_tick) {
    std::lock_guard<std::mutex> lock(g_mutex);
    g_state.scene_sequence_started = true;
    g_state.scene_sequence_complete = false;
    g_state.scene_generation = scene_generation;
    g_state.scene_start_tick = start_tick;
    g_state.last_tick = start_tick;
    update_foreground_phase_locked();
}

void on_frame(std::uint64_t tick, bool scene_sequence_complete) {
    std::lock_guard<std::mutex> lock(g_mutex);
    g_state.last_tick = tick;
    if (g_state.scene_sequence_started && scene_sequence_complete) {
        g_state.scene_sequence_complete = true;
    }
    update_foreground_phase_locked();
}

Snapshot snapshot() {
    std::lock_guard<std::mutex> lock(g_mutex);
    return g_state;
}

const char* phase_name(Phase phase) {
    switch (phase) {
        case Phase::kCold: return "cold";
        case Phase::kRuntimeConfigured: return "runtime-configured";
        case Phase::kSurfaceReady: return "surface-ready";
        case Phase::kForegroundReady: return "foreground-ready";
        case Phase::kSplashRunning: return "splash-running";
        case Phase::kInitialUiReady: return "initial-ui-ready";
        case Phase::kBackground: return "background";
    }
    return "unknown";
}

std::string status_report() {
    const Snapshot state = snapshot();
    std::ostringstream out;
    out << "app delegate state\n";
    out << "phase: " << phase_name(state.phase) << "\n";
    out << "runtime configured: " << (state.runtime_configured ? "yes" : "no") << "\n";
    out << "surface ready: " << (state.surface_ready ? "yes" : "no")
        << " (generation " << state.surface_generation << ")\n";
    out << "resumed: " << (state.resumed ? "yes" : "no")
        << " (pause/resume " << state.pause_count << "/" << state.resume_count << ")\n";
    out << "scene sequence: "
        << (state.scene_sequence_started ? "started" : "not started")
        << (state.scene_sequence_complete ? ", complete" : ", incomplete")
        << " (generation " << state.scene_generation << ")\n";
    out << "scene ticks: start=" << state.scene_start_tick << ", last=" << state.last_tick << "\n";
    return out.str();
}

}  // namespace nevergone::app_delegate_state
