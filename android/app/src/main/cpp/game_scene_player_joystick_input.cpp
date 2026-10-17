#include "game_scene_player_joystick_input.h"

#include <algorithm>
#include <cmath>
#include <atomic>
#include <deque>
#include <iomanip>
#include <mutex>
#include <sstream>
#include <utility>

#include "controls_function_state_runtime.h"
#include "game_clock.h"
#include "game_scene_player_motion_state.h"
#include "minimal_runtime_save_runtime.h"
#include "meplayer_live_action_state.h"
#include "player_control_runtime_state.h"
#include "player_direction_combo_contract.h"
#include "player_motion_contract.h"
#include "tutorial_gameplay_state.h"

namespace nevergone::game_scene_player_joystick_input {
namespace {

constexpr float kPi = 3.14159265358979323846f;
constexpr int kActionDown = 0;
constexpr int kActionUp = 1;
constexpr int kActionMove = 2;
constexpr int kActionCancel = 3;
constexpr int kActionPointerDown = 5;
constexpr int kActionPointerUp = 6;

std::mutex g_mutex;
Snapshot g_snapshot;
std::mutex g_trace_mutex;
std::deque<std::string> g_trace;
std::atomic<std::uint64_t> g_trace_frame{0};
std::uint64_t g_trace_sequence = 0;
std::size_t g_post_input_updates = 0;
bool g_capture_active = false;
bool g_capture_frozen = false;
constexpr std::size_t kMaxTraceRows = 512;
constexpr std::size_t kPreInputRows = 128;
constexpr std::size_t kPostInputRows = kMaxTraceRows - kPreInputRows;

bool finite(float value) {
    return std::isfinite(value);
}

float combo_elapsed_seconds_locked(std::uint64_t game_tick) {
    if (g_snapshot.direction_combo_state == 0u ||
        game_tick < g_snapshot.direction_combo_started_tick) {
        return 0.0f;
    }
    return static_cast<float>(
            static_cast<double>(game_tick - g_snapshot.direction_combo_started_tick) *
            game_clock::kFixedStepSeconds);
}

void deactivate_direction_combo_locked(std::uint64_t game_tick) {
    const float elapsed = combo_elapsed_seconds_locked(game_tick);
    g_snapshot.direction_combo_released = true;
    g_snapshot.movement_action_state = player_motion_contract::kNormalMoveActionState;

    // Controls::onCCJoyStickDeactivated() preserves the short combo timer/state
    // so a second same-direction tap can be recognized, but clears both once
    // the retained +0x158 timer is strictly greater than 0.5 seconds.
    if (player_direction_combo_contract::expires_on_deactivate(elapsed)) {
        g_snapshot.direction_combo_state = 0u;
        g_snapshot.direction_combo_started_tick = game_tick;
    }
}

void release_capture_locked(std::uint64_t game_tick) {
    if (g_snapshot.captured) {
        deactivate_direction_combo_locked(game_tick);
    }
    g_snapshot.captured = false;
    g_snapshot.pointer_id = -1;
    g_snapshot.design_x = g_snapshot.center_x;
    g_snapshot.design_y = g_snapshot.center_y;
    g_snapshot.power = 0.0f;
    g_snapshot.angle_degrees = 0.0f;
}

void push_trace_row(std::string row) {
    g_trace.push_back(std::move(row));
    if (g_trace.size() > kMaxTraceRows) g_trace.pop_front();
}

void keep_pre_input_window() {
    std::size_t updates = 0;
    for (auto it = g_trace.rbegin(); it != g_trace.rend(); ++it) {
        if (it->find(" U ") != std::string::npos) ++updates;
        if (updates > kPreInputRows) {
            g_trace.erase(g_trace.begin(), it.base());
            break;
        }
    }
    g_trace.erase(
        std::remove_if(g_trace.begin(), g_trace.end(), [](const std::string& row) {
            return row.find(" I ") != std::string::npos;
        }),
        g_trace.end());
}

bool update_sample_locked(float design_x, float design_y, bool count_move) {
    const float local_x = design_x - g_snapshot.center_x;
    const float local_y = design_y - g_snapshot.center_y;
    const float length = std::sqrt(local_x * local_x + local_y * local_y);
    if (!finite(length) || length > kUpdateRadius) return false;

    g_snapshot.design_x = design_x;
    g_snapshot.design_y = design_y;
    g_snapshot.power = length / kUpdateRadius;
    g_snapshot.angle_degrees = length > 0.0f
        ? std::atan2(local_y, local_x) * (180.0f / kPi)
        : 0.0f;
    if (count_move) ++g_snapshot.accepted_moves;
    return true;
}

void update_horizontal_combo_locked(float angle_degrees, std::uint64_t game_tick) {
    namespace combo = player_direction_combo_contract;
    const auto side = combo::horizontal_side(angle_degrees);
    const auto normal = combo::normal_state(side);
    if (!normal.has_value()) return;

    float elapsed = combo_elapsed_seconds_locked(game_tick);

    // A recovered fast direction-combo state remains visible at Controls +0x110
    // until its +0x158 timer reaches 0.4 seconds. MEPlayer action state 1 stays
    // latched while the current touch remains active even after +0x110 clears.
    if (combo::is_fast_state(g_snapshot.direction_combo_state)) {
        if (elapsed >= combo::kFastWindowMaxSeconds) {
            g_snapshot.direction_combo_state = 0u;
            g_snapshot.direction_combo_started_tick = game_tick;
            g_snapshot.direction_combo_released = false;
        } else {
            return;
        }
    }

    if (g_snapshot.direction_combo_state == 0u) {
        g_snapshot.direction_combo_state = *normal;
        g_snapshot.direction_combo_started_tick = game_tick;
        g_snapshot.direction_combo_released = false;
        // Do not downgrade an already-latched action-state-1 run while the same
        // touch is still held after the native combo-state timer expires.
        return;
    }

    elapsed = combo_elapsed_seconds_locked(game_tick);
    if (g_snapshot.direction_combo_released) {
        const auto fast = combo::fast_reactivation_state(
                g_snapshot.direction_combo_state, side, elapsed);
        if (fast.has_value()) {
            g_snapshot.direction_combo_state = *fast;
            g_snapshot.direction_combo_started_tick = game_tick;
            g_snapshot.movement_action_state = player_motion_contract::kFastMoveActionState;
            ++g_snapshot.fast_reactivation_count;
            return;
        }

        // Opposite-direction transitions (native states 0x0e/0x10) are a
        // separate combo boundary. Fail closed to ordinary movement instead of
        // inventing their action semantics.
        g_snapshot.direction_combo_state = *normal;
        g_snapshot.direction_combo_started_tick = game_tick;
        g_snapshot.direction_combo_released = false;
        g_snapshot.movement_action_state = player_motion_contract::kNormalMoveActionState;
        return;
    }

    if (!combo::same_normal_side(g_snapshot.direction_combo_state, side)) {
        // Direction changed while still captured. Preserve the already-proven
        // fast action latch if one exists; otherwise restart the normal timer for
        // the new horizontal sector without assigning cross-direction actions.
        g_snapshot.direction_combo_state = *normal;
        g_snapshot.direction_combo_started_tick = game_tick;
    }
}

}  // namespace

void record_frame_sample(bool controls_allowed) {
    if (!controls_allowed) {
        return;
    }
    std::lock_guard<std::mutex> lock(g_trace_mutex);
    const std::uint64_t render_frame =
        g_trace_frame.fetch_add(1, std::memory_order_relaxed) + 1;
    if (g_capture_frozen) return;
    if (g_capture_active && g_post_input_updates >= kPostInputRows) {
        g_capture_active = false;
        g_capture_frozen = true;
        return;
    }
    const std::uint64_t game_tick = game_clock::tick_count();
    const auto motion = game_scene_player_motion_state::snapshot();
    const Snapshot joystick = snapshot();
    const auto control = player_control_runtime_state::snapshot();
    const auto function = controls_function_state_runtime::snapshot();
    const auto lesson = tutorial_gameplay_state::snapshot();

    std::ostringstream row;
    row << "U " << render_frame << ' ' << game_tick << ' '
        << 1 << ' ' << (joystick.captured ? 1 : 0) << ' '
        << joystick.pointer_id << " 0x" << std::hex << joystick.direction_combo_state
        << ' ' << (joystick.direction_combo_released ? 1 : 0)
        << " 0x" << joystick.movement_action_state << std::dec << ' '
        << (control.primary_state.has_value()
                ? static_cast<int>(*control.primary_state) : 0)
        << ' ' << (control.flip_x ? 1 : 0) << " 0x" << std::hex
        << function.function_state << std::dec << ' '
        << motion.applied_steps << ' ' << std::setprecision(7)
        << motion.offset_x << ' ' << motion.offset_y << ' '
        << tutorial_gameplay_state::step_name(lesson.step);
    push_trace_row(std::to_string(++g_trace_sequence) + " " + row.str());
    if (g_capture_active) {
        ++g_post_input_updates;
        if (g_post_input_updates >= kPostInputRows) {
            g_capture_active = false;
            g_capture_frozen = true;
        }
    } else {
        std::size_t updates = 0;
        for (const auto& trace_row : g_trace) {
            if (trace_row.find(" U ") != std::string::npos) ++updates;
        }
        while (updates > kPreInputRows && !g_trace.empty()) {
            const bool was_update = g_trace.front().find(" U ") != std::string::npos;
            g_trace.pop_front();
            if (was_update) --updates;
        }
    }
}

void record_input_event(int action, int pointer_id, float surface_x, float surface_y) {
    std::lock_guard<std::mutex> lock(g_trace_mutex);
    if (!g_capture_active) keep_pre_input_window();
    std::ostringstream row;
    row << "I " << g_trace_frame.load(std::memory_order_relaxed) << ' '
        << game_clock::tick_count() << ' '
        << action << ' ' << pointer_id << ' ' << std::setprecision(7)
        << surface_x << ' ' << surface_y;

    push_trace_row(std::to_string(++g_trace_sequence) + " " + row.str());
    if (action == kActionDown || action == kActionUp || action == kActionMove ||
            action == kActionCancel) {
        g_capture_active = true;
        g_capture_frozen = false;
        g_post_input_updates = 0;
    }
}

bool surface_to_design(
        int surface_width,
        int surface_height,
        float surface_x,
        float surface_y,
        float* design_x,
        float* design_y) {
    if (design_x == nullptr || design_y == nullptr ||
        surface_width <= 0 || surface_height <= 0 ||
        !finite(surface_x) || !finite(surface_y)) {
        return false;
    }
    const float width = static_cast<float>(surface_width);
    const float height = static_cast<float>(surface_height);
    if (surface_x < 0.0f || surface_x > width ||
        surface_y < 0.0f || surface_y > height) {
        return false;
    }

    *design_x = surface_x * kDesignWidth / width;
    *design_y = kDesignHeight - surface_y * kDesignHeight / height;
    return finite(*design_x) && finite(*design_y);
}

bool set_center(float design_x, float design_y) {
    if (!finite(design_x) || !finite(design_y)) return false;
    std::lock_guard<std::mutex> lock(g_mutex);
    if (g_snapshot.captured) release_capture_locked(game_clock::tick_count());
    g_snapshot.center_x = design_x;
    g_snapshot.center_y = design_y;
    g_snapshot.design_x = design_x;
    g_snapshot.design_y = design_y;
    return true;
}

bool on_touch(
        int action,
        int pointer_id,
        float surface_x,
        float surface_y,
        int surface_width,
        int surface_height) {
    float design_x = 0.0f;
    float design_y = 0.0f;
    const bool mapped = surface_to_design(
        surface_width, surface_height, surface_x, surface_y, &design_x, &design_y);

    std::lock_guard<std::mutex> lock(g_mutex);
    const std::uint64_t game_tick = game_clock::tick_count();
    if (action == kActionCancel) {
        if (!g_snapshot.captured) return false;
        release_capture_locked(game_tick);
        return true;
    }

    if (action == kActionDown || action == kActionPointerDown) {
        if (g_snapshot.captured || !mapped) return false;
        const float dx = design_x - g_snapshot.center_x;
        const float dy = design_y - g_snapshot.center_y;
        const float distance = std::sqrt(dx * dx + dy * dy);
        if (!finite(distance) || distance >= kHitRadius) return false;
        g_snapshot.captured = true;
        g_snapshot.pointer_id = pointer_id;
        g_snapshot.movement_action_state = player_motion_contract::kNormalMoveActionState;
        ++g_snapshot.accepted_begins;
        (void)update_sample_locked(design_x, design_y, false);
        return true;
    }

    if (!g_snapshot.captured || pointer_id != g_snapshot.pointer_id) return false;

    if (action == kActionMove) {
        if (mapped) (void)update_sample_locked(design_x, design_y, true);
        return true;
    }

    if (action == kActionUp || action == kActionPointerUp) {
        release_capture_locked(game_tick);
        return true;
    }

    return false;
}

void tick() {
    const std::uint64_t game_tick = game_clock::tick_count();
    float power = 0.0f;
    float angle = 0.0f;
    float direction_x = 0.0f;
    float direction_y = 0.0f;
    bool captured = false;
    bool run_performed = false;
    std::uint32_t movement_action_state = player_motion_contract::kNormalMoveActionState;
    {
        std::lock_guard<std::mutex> lock(g_mutex);
        captured = g_snapshot.captured;
        if (captured) {
            power = g_snapshot.power;
            angle = g_snapshot.angle_degrees;
            const float local_x = g_snapshot.design_x - g_snapshot.center_x;
            const float local_y = g_snapshot.design_y - g_snapshot.center_y;
            const float length = std::sqrt(local_x * local_x + local_y * local_y);
            if (finite(length) && length > 0.0f) {
                direction_x = local_x / length;
                direction_y = local_y / length;
            }
            if (power > 0.05f) {
                const std::uint64_t before_fast_reactivations =
                    g_snapshot.fast_reactivation_count;
                update_horizontal_combo_locked(angle, game_tick);
                run_performed =
                    g_snapshot.fast_reactivation_count > before_fast_reactivations;
            }
            movement_action_state = g_snapshot.movement_action_state;
        }
    }

    // A recovered fast same-direction reactivation is the concrete gameplay
    // outcome requested by the original Run lesson. Publish it outside the
    // joystick mutex so tutorial progression can never participate in the
    // input lock order. Ignore it when another tutorial step owns the surface.
    if (run_performed) {
        const auto tutorial = tutorial_gameplay_state::snapshot();
        if (tutorial.step == tutorial_gameplay_state::Step::kRun) {
            (void)tutorial_gameplay_state::observe(
                tutorial_gameplay_state::Event::kRunPerformed);
        }
    }

    // Consuming every fixed tick, including neutral ticks, prevents a later
    // joystick activation from applying movement accumulated while idle.
    bool active = false;
    if (captured) {
        // Runtime capture proves Controls +0x168 is the normalized direction and
        // +0x160 is the separate activity sample used by the 0.05 threshold.
        active = player_control_runtime_state::apply_horizontal_sample(power, angle);
        if (active) {
            (void)meplayer_live_action_state::observe_movement_action_state(
                movement_action_state, true);
        } else if (power <= 0.05f) {
            (void)meplayer_live_action_state::observe_movement_action_state(
                player_motion_contract::kNormalMoveActionState, false);
        }
    } else {
        (void)meplayer_live_action_state::observe_movement_action_state(
            player_motion_contract::kNormalMoveActionState, false);
    }
    (void)game_scene_player_motion_state::advance_sample(
            game_tick,
            active,
            movement_action_state,
            direction_x,
            direction_y);
}

void cancel_capture() {
    std::lock_guard<std::mutex> lock(g_mutex);
    release_capture_locked(game_clock::tick_count());
}

void reset() {
    {
        std::lock_guard<std::mutex> lock(g_mutex);
        const auto accepted_begins = g_snapshot.accepted_begins;
        const auto accepted_moves = g_snapshot.accepted_moves;
        const float center_x = g_snapshot.center_x;
        const float center_y = g_snapshot.center_y;
        g_snapshot = {};
        g_snapshot.accepted_begins = accepted_begins;
        g_snapshot.accepted_moves = accepted_moves;
        g_snapshot.center_x = center_x;
        g_snapshot.center_y = center_y;
        g_snapshot.design_x = center_x;
        g_snapshot.design_y = center_y;
    }
    player_control_runtime_state::reset();
    game_scene_player_motion_state::reset();
}

Snapshot snapshot() {
    std::lock_guard<std::mutex> lock(g_mutex);
    return g_snapshot;
}

std::string status_report() {
    const Snapshot state = snapshot();
    std::ostringstream out;
    out << "GameScene player joystick\n";
    out << "captured: " << (state.captured ? "true" : "false") << "\n";
    out << "pointer id: " << state.pointer_id << "\n";
    out << "accepted begins: " << state.accepted_begins << "\n";
    out << "accepted moves: " << state.accepted_moves << "\n";
    out << "design point: " << state.design_x << "," << state.design_y << "\n";
    out << "power: " << state.power << "\n";
    out << "angle degrees: " << state.angle_degrees << "\n";
    out << "direction combo state: 0x" << std::hex << state.direction_combo_state << std::dec << "\n";
    out << "direction combo released: " << (state.direction_combo_released ? "true" : "false") << "\n";
    out << "movement action state: 0x" << std::hex << state.movement_action_state << std::dec << "\n";
    out << "fast reactivations: " << state.fast_reactivation_count << "\n";
    out << game_scene_player_motion_state::status_report();
    {
        std::lock_guard<std::mutex> lock(g_trace_mutex);
        out << "Player tick trace (seq kind frame game_tick; U then controls_allowed captured pointer combo released move_action primary flip function steps x y lesson; I then action pointer x y)\n";
        out << "retained rows: " << g_trace.size() << "\n";
        for (const auto& row : g_trace) out << row << "\n";
    }
    out << nevergone::minimal_runtime_save_runtime::status_report();
    return out.str();
}

}  // namespace nevergone::game_scene_player_joystick_input
