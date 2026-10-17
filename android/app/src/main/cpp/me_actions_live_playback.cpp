#include "me_actions_live_playback.h"

#include <mutex>
#include <sstream>

#include "game_clock.h"
#include "me_actions_combo_boundaries.h"

namespace nevergone::me_actions_live_playback {
namespace {

std::mutex g_mutex;
Snapshot g_snapshot{};
me_actions_combo_boundaries::Boundaries g_boundaries{};

bool same_action_locked(
        std::uint64_t action_generation,
        std::uint64_t loaded_revision,
        std::uint32_t action_state,
        bool looping) {
    return g_snapshot.ready &&
           g_snapshot.action_generation == action_generation &&
           g_snapshot.loaded_revision == loaded_revision &&
           g_snapshot.action_state == action_state &&
           g_snapshot.looping == looping;
}

void begin_action_locked(
        std::uint64_t action_generation,
        std::uint64_t loaded_revision,
        std::uint32_t action_state,
        bool looping,
        const enemy_actions_wbg_document::Document& document,
        std::uint64_t game_tick) {
    const std::uint64_t prior_frame_revision = g_snapshot.frame_revision;
    const std::uint64_t prior_completed = g_snapshot.completed_action_count;
    g_snapshot = {};
    g_snapshot.ready = true;
    g_snapshot.action_generation = action_generation;
    g_snapshot.loaded_revision = loaded_revision;
    g_snapshot.action_state = action_state;
    g_snapshot.looping = looping;
    g_snapshot.last_game_tick = game_tick;
    g_snapshot.frame_revision = prior_frame_revision + 1u;
    g_snapshot.completed_action_count = prior_completed;
    g_snapshot.cursor = me_actions_playback_timing::initial_cursor();
    g_boundaries = me_actions_combo_boundaries::derive(document.combo_block);
}

}  // namespace

void reset() {
    std::lock_guard<std::mutex> lock(g_mutex);
    const std::uint64_t revision = g_snapshot.frame_revision + 1u;
    g_snapshot = {};
    g_snapshot.frame_revision = revision;
    g_boundaries = {};
}

bool sync_and_tick(
        std::uint64_t action_generation,
        std::uint64_t loaded_revision,
        std::uint32_t action_state,
        bool looping,
        const enemy_actions_wbg_document::Document& document,
        std::uint64_t game_tick) {
    if (action_generation == 0u || loaded_revision == 0u ||
        document.primary_records.empty() || document.final_table.entries.empty()) {
        return false;
    }

    std::lock_guard<std::mutex> lock(g_mutex);
    if (!same_action_locked(action_generation, loaded_revision, action_state, looping)) {
        begin_action_locked(
            action_generation, loaded_revision, action_state, looping, document, game_tick);
        return true;
    }

    // game_clock may reset on surface recreation. Restart the logical playback
    // tick baseline rather than underflowing a delta or replaying elapsed time.
    if (game_tick < g_snapshot.last_game_tick) {
        g_snapshot.last_game_tick = game_tick;
        return true;
    }
    if (game_tick == g_snapshot.last_game_tick || g_snapshot.cursor.timing.done_220) {
        return true;
    }

    const std::uint64_t elapsed_ticks = game_tick - g_snapshot.last_game_tick;
    g_snapshot.last_game_tick = game_tick;
    const float delta_seconds = static_cast<float>(game_clock::kFixedStepSeconds);

    // Preserve native step() boundaries when rendering skips logical ticks:
    // a completed loop restarts before the next elapsed tick is processed.
    for (std::uint64_t tick = 0; tick < elapsed_ticks; ++tick) {
        if (g_snapshot.cursor.timing.done_220) break;
        const std::int32_t previous_frame = g_snapshot.cursor.current_frame_1a4;
        const bool was_done = g_snapshot.cursor.timing.done_220;
        const auto update = me_actions_playback_timing::apply_wa_update_playback(
            document, g_boundaries, delta_seconds, g_snapshot.cursor);
        if (!update.valid_projection) return false;

        g_snapshot.cursor = update.cursor;
        ++g_snapshot.update_count;
        if (g_snapshot.cursor.current_frame_1a4 != previous_frame ||
            update.frames_advanced > 0u) {
            ++g_snapshot.frame_revision;
        }
        if (!was_done && g_snapshot.cursor.timing.done_220) {
            ++g_snapshot.completed_action_count;
            if (g_snapshot.looping) {
                g_snapshot.cursor = me_actions_playback_timing::initial_cursor();
                // Native step() immediately calls play() + waStep() after a
                // loop completes, clearing the reset flag in the same tick.
                g_snapshot.cursor.timing.reset_accumulator_1bc = false;
                ++g_snapshot.update_count;
                ++g_snapshot.frame_revision;
            }
        }
    }
    return true;
}

Snapshot snapshot() {
    std::lock_guard<std::mutex> lock(g_mutex);
    return g_snapshot;
}

me_actions_runtime_state::State runtime_state() {
    const Snapshot state = snapshot();
    me_actions_runtime_state::State runtime;
    runtime.current_frame_1a4 = state.cursor.current_frame_1a4;
    runtime.previous_frame_1a8 = state.cursor.previous_frame_1a8;
    return runtime;
}

std::string status_report() {
    const Snapshot state = snapshot();
    std::ostringstream out;
    out << "MeActions live playback\n";
    out << "ready: " << (state.ready ? "yes" : "no") << "\n";
    out << "action generation: " << state.action_generation << "\n";
    out << "loaded revision: " << state.loaded_revision << "\n";
    out << "action state: 0x" << std::hex << state.action_state << std::dec << "\n";
    out << "looping: " << (state.looping ? "yes" : "no") << "\n";
    out << "current frame: " << state.cursor.current_frame_1a4 << "\n";
    out << "previous frame: " << state.cursor.previous_frame_1a8 << "\n";
    out << "frame revision: " << state.frame_revision << "\n";
    out << "updates: " << state.update_count << "\n";
    out << "done: " << (state.cursor.timing.done_220 ? "yes" : "no") << "\n";
    out << "completed actions: " << state.completed_action_count << "\n";
    return out.str();
}

}  // namespace nevergone::me_actions_live_playback
