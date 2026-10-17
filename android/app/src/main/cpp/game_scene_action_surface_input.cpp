#include "game_scene_action_surface_input.h"

#include <cmath>
#include <mutex>
#include <sstream>
#include <unordered_map>
#include <vector>

namespace nevergone::game_scene_action_surface_input {
namespace {

constexpr int kActionDown = 0;
constexpr int kActionUp = 1;
constexpr int kActionMove = 2;
constexpr int kActionCancel = 3;
constexpr int kActionPointerDown = 5;
constexpr int kActionPointerUp = 6;
constexpr float kDesignWidth = 1136.0f;
constexpr float kDesignHeight = 640.0f;

std::mutex g_mutex;
Snapshot g_snapshot;
std::unordered_map<int, game_scene_action_input::Control> g_captures;

int slot_for(game_scene_action_input::Control control) {
    switch (control) {
        case game_scene_action_input::Control::kButtonA: return 0;
        case game_scene_action_input::Control::kButtonB: return 1;
        case game_scene_action_input::Control::kButtonC: return 2;
        case game_scene_action_input::Control::kNone: return -1;
    }
    return -1;
}

void refresh_pressed_locked() {
    g_snapshot.pressed = {};
    for (const auto& entry : g_captures) {
        const int slot = slot_for(entry.second);
        if (slot >= 0) g_snapshot.pressed[static_cast<std::size_t>(slot)] = true;
    }
}

bool map_surface(
        int surface_width,
        int surface_height,
        float surface_x,
        float surface_y,
        float* design_x,
        float* design_y) {
    if (design_x == nullptr || design_y == nullptr ||
        surface_width <= 0 || surface_height <= 0 ||
        !std::isfinite(surface_x) || !std::isfinite(surface_y)) {
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
    return std::isfinite(*design_x) && std::isfinite(*design_y);
}

bool valid_point(game_scene_action_layout::Point point) {
    return std::isfinite(point.x) && std::isfinite(point.y);
}

game_scene_action_input::Control classify_locked(float design_x, float design_y) {
    return game_scene_action_layout::hit_test(
        kDesignWidth,
        design_x,
        design_y,
        g_snapshot.sprite_bounds,
        g_snapshot.positions);
}

void publish_release(game_scene_action_input::Control control, bool tapped) {
    (void)game_scene_action_input::observe(
        control,
        game_scene_action_input::Gesture::kRelease);
    if (tapped) {
        (void)game_scene_action_input::observe(
            control,
            game_scene_action_input::Gesture::kTap);
    }
}

}  // namespace

void reset() {
    std::vector<game_scene_action_input::Control> captured;
    {
        std::lock_guard<std::mutex> lock(g_mutex);
        captured.reserve(g_captures.size());
        for (const auto& entry : g_captures) captured.push_back(entry.second);
        g_captures.clear();
        const auto positions = g_snapshot.positions;
        g_snapshot = {};
        g_snapshot.positions = positions;
    }
    for (const auto control : captured) publish_release(control, false);
}

bool set_sprite_bounds(int slot, int width, int height) {
    if (slot < 0 || slot >= game_scene_action_layout::kButtonCount ||
        width <= 0 || height <= 0 || width > 4096 || height > 4096) {
        return false;
    }
    std::lock_guard<std::mutex> lock(g_mutex);
    auto& bounds = g_snapshot.sprite_bounds[static_cast<std::size_t>(slot)];
    bounds.width = static_cast<float>(width);
    bounds.height = static_cast<float>(height);
    ++g_snapshot.bounds_revision;
    return true;
}

void clear_sprite_bounds() {
    std::lock_guard<std::mutex> lock(g_mutex);
    g_snapshot.sprite_bounds = {};
    ++g_snapshot.bounds_revision;
}

bool apply_saved_control_settings(
    bool enabled,
    game_scene_action_layout::Point joystick,
    game_scene_action_layout::Point button_b,
    game_scene_action_layout::Point unique_skill_tip) {
    if (enabled && (!valid_point(joystick) || !valid_point(button_b) ||
                    !valid_point(unique_skill_tip))) {
        return false;
    }
    std::lock_guard<std::mutex> lock(g_mutex);
    g_snapshot.positions = game_scene_action_layout::positions_from_saved_settings(
        kDesignWidth,
        enabled,
        joystick,
        button_b,
        unique_skill_tip);
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
    const bool mapped = map_surface(
        surface_width,
        surface_height,
        surface_x,
        surface_y,
        &design_x,
        &design_y);

    if (action == kActionCancel) {
        std::vector<game_scene_action_input::Control> captured;
        {
            std::lock_guard<std::mutex> lock(g_mutex);
            if (g_captures.empty()) return false;
            captured.reserve(g_captures.size());
            for (const auto& entry : g_captures) captured.push_back(entry.second);
            g_snapshot.cancelled_captures += g_captures.size();
            g_captures.clear();
            g_snapshot.captured_pointer_count = 0;
            refresh_pressed_locked();
        }
        for (const auto control : captured) publish_release(control, false);
        return true;
    }

    if (action == kActionDown || action == kActionPointerDown) {
        if (!mapped) return false;
        game_scene_action_input::Control control;
        {
            std::lock_guard<std::mutex> lock(g_mutex);
            if (g_captures.find(pointer_id) != g_captures.end()) return false;
            control = classify_locked(design_x, design_y);
            if (control == game_scene_action_input::Control::kNone) return false;
            g_captures.emplace(pointer_id, control);
            ++g_snapshot.accepted_begins;
            g_snapshot.captured_pointer_count = static_cast<int>(g_captures.size());
            refresh_pressed_locked();
        }
        (void)game_scene_action_input::observe(
            control,
            game_scene_action_input::Gesture::kPress);
        return true;
    }

    game_scene_action_input::Control captured_control;
    {
        std::lock_guard<std::mutex> lock(g_mutex);
        const auto found = g_captures.find(pointer_id);
        if (found == g_captures.end()) return false;
        captured_control = found->second;
    }

    if (action == kActionMove) return true;

    if (action == kActionUp || action == kActionPointerUp) {
        bool tapped = false;
        {
            std::lock_guard<std::mutex> lock(g_mutex);
            if (mapped) tapped = classify_locked(design_x, design_y) == captured_control;
            g_captures.erase(pointer_id);
            ++g_snapshot.accepted_ends;
            if (tapped) ++g_snapshot.taps;
            g_snapshot.captured_pointer_count = static_cast<int>(g_captures.size());
            refresh_pressed_locked();
        }
        publish_release(captured_control, tapped);
        return true;
    }

    return false;
}

void cancel_all_captures() {
    (void)on_touch(kActionCancel, -1, 0.0f, 0.0f, 1, 1);
}

Snapshot snapshot() {
    std::lock_guard<std::mutex> lock(g_mutex);
    return g_snapshot;
}

std::string status_report() {
    const Snapshot state = snapshot();
    std::ostringstream out;
    out << "GameScene action surface input\n";
    out << "action bounds revision: " << state.bounds_revision << "\n";
    for (std::size_t index = 0; index < state.sprite_bounds.size(); ++index) {
        out << "action sprite " << index << ": "
            << state.sprite_bounds[index].width << "x"
            << state.sprite_bounds[index].height
            << " pressed=" << (state.pressed[index] ? "yes" : "no") << "\n";
    }
    out << "action captures: " << state.captured_pointer_count << "\n";
    out << "action begins: " << state.accepted_begins << "\n";
    out << "action ends: " << state.accepted_ends << "\n";
    out << "action taps: " << state.taps << "\n";
    out << "action cancelled captures: " << state.cancelled_captures << "\n";
    return out.str();
}

}  // namespace nevergone::game_scene_action_surface_input
