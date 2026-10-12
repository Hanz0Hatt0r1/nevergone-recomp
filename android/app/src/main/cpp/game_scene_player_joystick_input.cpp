#include "game_scene_player_joystick_input.h"

#include <cmath>
#include <mutex>
#include <sstream>

#include "game_scene_player_motion_state.h"
#include "player_control_runtime_state.h"

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

bool finite(float value) {
    return std::isfinite(value);
}

void release_capture_locked() {
    g_snapshot.captured = false;
    g_snapshot.pointer_id = -1;
    g_snapshot.design_x = kCenterX;
    g_snapshot.design_y = kCenterY;
    g_snapshot.power = 0.0f;
    g_snapshot.angle_degrees = 0.0f;
}

bool update_sample_locked(float design_x, float design_y, bool count_move) {
    const float local_x = design_x - kCenterX;
    const float local_y = design_y - kCenterY;
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

}  // namespace

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
    if (action == kActionCancel) {
        // Android ACTION_CANCEL terminates the whole gesture, not merely the
        // pointer at actionIndex.
        if (!g_snapshot.captured) return false;
        release_capture_locked();
        return true;
    }

    if (action == kActionDown || action == kActionPointerDown) {
        if (g_snapshot.captured || !mapped) return false;
        const float dx = design_x - kCenterX;
        const float dy = design_y - kCenterY;
        const float distance = std::sqrt(dx * dx + dy * dy);
        // CCJoyStick::containsTouchLocation accepts only distance < radius.
        if (!finite(distance) || distance >= kHitRadius) return false;
        g_snapshot.captured = true;
        g_snapshot.pointer_id = pointer_id;
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
        release_capture_locked();
        return true;
    }

    return false;
}

void tick() {
    float power = 0.0f;
    float angle = 0.0f;
    float direction_x = 0.0f;
    float direction_y = 0.0f;
    {
        std::lock_guard<std::mutex> lock(g_mutex);
        if (!g_snapshot.captured) return;
        power = g_snapshot.power;
        angle = g_snapshot.angle_degrees;
        const float local_x = g_snapshot.design_x - kCenterX;
        const float local_y = g_snapshot.design_y - kCenterY;
        const float length = std::sqrt(local_x * local_x + local_y * local_y);
        if (finite(length) && length > 0.0f) {
            direction_x = local_x / length;
            direction_y = local_y / length;
        }
    }

    // The runtime capture confirms that Controls +0x168 retains this normalized
    // direction while +0x160 supplies the separate activity/power threshold.
    // A valid horizontal sample therefore both updates facing and advances the
    // evidence-backed normal locomotion cursor by one shipped movement tick.
    if (player_control_runtime_state::apply_horizontal_sample(power, angle)) {
        (void)game_scene_player_motion_state::apply_normal_direction(
                direction_x, direction_y);
    }
}

void cancel_capture() {
    std::lock_guard<std::mutex> lock(g_mutex);
    release_capture_locked();
}

void reset() {
    {
        std::lock_guard<std::mutex> lock(g_mutex);
        const auto accepted_begins = g_snapshot.accepted_begins;
        const auto accepted_moves = g_snapshot.accepted_moves;
        g_snapshot = {};
        g_snapshot.accepted_begins = accepted_begins;
        g_snapshot.accepted_moves = accepted_moves;
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
    return out.str();
}

}  // namespace nevergone::game_scene_player_joystick_input
