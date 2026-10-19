#include "tap_to_start_state.h"

#include <mutex>

namespace nevergone::tap_to_start_state {
namespace {

std::mutex g_mutex;
Phase g_phase = Phase::kInactive;
std::uint64_t g_scene_generation = 0;
bool g_auto_login_request_pending = false;
std::uint64_t g_accepted_touches = 0;

}  // namespace

void reset() {
    std::lock_guard<std::mutex> lock(g_mutex);
    g_phase = Phase::kInactive;
    g_scene_generation = 0;
    g_auto_login_request_pending = false;
    g_accepted_touches = 0;
}

void sync_scene(std::uint64_t scene_generation, bool single_login_active) {
    std::lock_guard<std::mutex> lock(g_mutex);
    const std::uint64_t previous_generation = g_scene_generation;
    if (previous_generation != scene_generation) {
        g_scene_generation = scene_generation;
        g_auto_login_request_pending = false;
        g_phase = single_login_active ? Phase::kReady : Phase::kInactive;
        return;
    }

    if (!single_login_active) {
        g_auto_login_request_pending = false;
        g_phase = Phase::kInactive;
        return;
    }

    if (g_phase == Phase::kInactive) g_phase = Phase::kReady;
}

bool touch_began() {
    std::lock_guard<std::mutex> lock(g_mutex);
    if (g_phase != Phase::kReady) return false;
    g_auto_login_request_pending = true;
    g_phase = Phase::kAutoLoginPending;
    ++g_accepted_touches;
    return true;
}

bool consume_auto_login_request() {
    std::lock_guard<std::mutex> lock(g_mutex);
    const bool pending = g_auto_login_request_pending;
    g_auto_login_request_pending = false;
    return pending;
}

Snapshot snapshot() {
    std::lock_guard<std::mutex> lock(g_mutex);
    Snapshot value;
    value.phase = g_phase;
    value.scene_generation = g_scene_generation;
    value.auto_login_request_pending = g_auto_login_request_pending;
    value.accepted_touches = g_accepted_touches;
    return value;
}

}  // namespace nevergone::tap_to_start_state
