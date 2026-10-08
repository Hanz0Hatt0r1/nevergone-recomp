#include "tap_to_start_state.h"

#include <atomic>

namespace nevergone::tap_to_start_state {
namespace {

std::atomic<int> g_phase{static_cast<int>(Phase::kInactive)};
std::atomic<std::uint64_t> g_scene_generation{0};
std::atomic<bool> g_auto_login_request_pending{false};
std::atomic<std::uint64_t> g_accepted_touches{0};

}  // namespace

void reset() {
    g_phase.store(static_cast<int>(Phase::kInactive), std::memory_order_release);
    g_scene_generation.store(0, std::memory_order_release);
    g_auto_login_request_pending.store(false, std::memory_order_release);
    g_accepted_touches.store(0, std::memory_order_release);
}

void sync_scene(std::uint64_t scene_generation, bool single_login_active) {
    const std::uint64_t previous_generation =
        g_scene_generation.load(std::memory_order_acquire);
    if (previous_generation != scene_generation) {
        g_scene_generation.store(scene_generation, std::memory_order_release);
        g_auto_login_request_pending.store(false, std::memory_order_release);
        g_phase.store(
            static_cast<int>(single_login_active ? Phase::kReady : Phase::kInactive),
            std::memory_order_release);
        return;
    }

    if (!single_login_active) {
        g_auto_login_request_pending.store(false, std::memory_order_release);
        g_phase.store(static_cast<int>(Phase::kInactive), std::memory_order_release);
        return;
    }

    int expected = static_cast<int>(Phase::kInactive);
    g_phase.compare_exchange_strong(
        expected,
        static_cast<int>(Phase::kReady),
        std::memory_order_acq_rel,
        std::memory_order_acquire);
}

bool touch_began() {
    int expected = static_cast<int>(Phase::kReady);
    if (!g_phase.compare_exchange_strong(
            expected,
            static_cast<int>(Phase::kAutoLoginPending),
            std::memory_order_acq_rel,
            std::memory_order_acquire)) {
        return false;
    }

    g_auto_login_request_pending.store(true, std::memory_order_release);
    g_accepted_touches.fetch_add(1, std::memory_order_relaxed);
    return true;
}

bool consume_auto_login_request() {
    return g_auto_login_request_pending.exchange(false, std::memory_order_acq_rel);
}

Snapshot snapshot() {
    Snapshot value;
    value.phase = static_cast<Phase>(g_phase.load(std::memory_order_acquire));
    value.scene_generation = g_scene_generation.load(std::memory_order_acquire);
    value.auto_login_request_pending =
        g_auto_login_request_pending.load(std::memory_order_acquire);
    value.accepted_touches = g_accepted_touches.load(std::memory_order_relaxed);
    return value;
}

}  // namespace nevergone::tap_to_start_state
