#include <jni.h>

#include <array>
#include <cstddef>
#include <cstdint>

#include "game_clock.h"
#include "single_login_lightning_timeline.h"
#include "splash_sequence_state.h"

namespace nevergone::single_login_thunder_bridge {
namespace {

constexpr std::size_t kQueueCapacity = single_login_lightning_timeline::kLightningCount;
std::array<bool, single_login_lightning_timeline::kLightningCount> g_active{};
std::array<int, kQueueCapacity> g_queue{};
std::size_t g_queue_head = 0;
std::size_t g_queue_size = 0;
std::uint64_t g_generation = 0;

void clear_state(std::uint64_t generation) {
    g_active = {};
    g_queue = {};
    g_queue_head = 0;
    g_queue_size = 0;
    g_generation = generation;
}

void enqueue(int sound_index) {
    if (sound_index < 0 || g_queue_size >= g_queue.size()) return;
    const std::size_t slot = (g_queue_head + g_queue_size) % g_queue.size();
    g_queue[slot] = sound_index;
    ++g_queue_size;
}

void capture_new_onsets() {
    const std::uint64_t generation = splash_sequence_state::generation();
    if (generation != g_generation) clear_state(generation);

    const std::uint64_t tick = game_clock::tick_count();
    const double scene_seconds = splash_sequence_state::single_login_seconds(tick);
    if (scene_seconds < 0.0) {
        g_active = {};
        return;
    }

    const auto sample = single_login_lightning_timeline::sample(scene_seconds);
    for (std::size_t index = 0; index < sample.strikes.size(); ++index) {
        const bool active = sample.strikes[index].onset;
        if (active && !g_active[index]) enqueue(sample.strikes[index].thunder_sound_index);
        g_active[index] = active;
    }
}

int poll() {
    capture_new_onsets();
    if (g_queue_size == 0) return -1;
    const int result = g_queue[g_queue_head];
    g_queue_head = (g_queue_head + 1) % g_queue.size();
    --g_queue_size;
    return result;
}

}  // namespace
}  // namespace nevergone::single_login_thunder_bridge

extern "C" JNIEXPORT jint JNICALL
Java_org_nevergone_recomp_GameSurfaceView_nativePollSingleLoginThunderSound(JNIEnv*, jclass) {
    return static_cast<jint>(nevergone::single_login_thunder_bridge::poll());
}
