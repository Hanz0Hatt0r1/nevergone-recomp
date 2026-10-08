#pragma once

#include <cstdint>
#include <string>

namespace nevergone::app_delegate_state {

enum class Phase {
    kCold,
    kRuntimeConfigured,
    kSurfaceReady,
    kForegroundReady,
    kSplashRunning,
    kInitialUiReady,
    kBackground,
};

struct Snapshot {
    Phase phase = Phase::kCold;
    bool runtime_configured = false;
    bool surface_ready = false;
    bool resumed = false;
    bool scene_sequence_started = false;
    bool scene_sequence_complete = false;
    std::uint64_t surface_generation = 0;
    std::uint64_t scene_generation = 0;
    std::uint64_t scene_start_tick = 0;
    std::uint64_t last_tick = 0;
    std::uint64_t pause_count = 0;
    std::uint64_t resume_count = 0;
};

void reset();
void on_runtime_configured();
void on_surface_ready();
void on_app_pause();
void on_app_resume();
void on_scene_sequence_reset(std::uint64_t scene_generation);
void on_scene_sequence_begin(std::uint64_t scene_generation, std::uint64_t start_tick);
void on_frame(std::uint64_t tick, bool scene_sequence_complete);
Snapshot snapshot();
const char* phase_name(Phase phase);
std::string status_report();

}  // namespace nevergone::app_delegate_state
