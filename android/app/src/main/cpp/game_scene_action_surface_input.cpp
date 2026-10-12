#include "game_scene_action_surface_input.h"

#include <cmath>
#include <mutex>
#include <sstream>
#include <unordered_map>
#include <vector>

#include "game_scene_player_joystick_input.h"

namespace nevergone::game_scene_action_surface_input {
namespace {

constexpr int kActionDown = 0;
constexpr int kActionUp = 1;
constexpr int kActionMove = 2;
constexpr int kActionCancel = 3;
constexpr int kActionPointerDown = 5;
constexpr int kActionPointerUp = 6;
constexpr float kDesignWidth = 1136.0f;

std::mutex g_mutex;
Snapshot g_snapshot;
std::unordered_map<int, game_scene_action_input::Control> g_captures;

bool map_surface(
        int surface_width,
        int surface_height,
        float surface_x,
        float surface_y,
        float* design_x,
        float* design_y) {
    return game_scene_player_joystick_input::surface_to_design(
        surface_width,
        surface_height,
        surface_x,
        surface_y,
        design_x,
        design_y);
}

game_scene_action_input::Control classify_locked(float design_x, float design_y) {
    return game_scene_action_layout::hit_test(
        kDesignWidth,
        design_x,
        design_y,
        g_snapshot.sprite_bounds);
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
        g_snapshot = {};
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

    if (action == kActionMove) {
        return true;
    }

    if (action == kActionUp || action == kActionPointerUp) {
        bool tapped = false;
        {
            std::lock_guard<std::mutex> lock(g_mutex);
            if (mapped) tapped = classify_locked(design_x, design_y) == captured_control;
            g_captures.erase(pointer_id);
            ++g_snapshot.accepted_ends;
            if (tapped) ++g_snapshot.taps;
            g_snapshot.captured_pointer_count = static_cast<int>(g_captures.size());
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
            << state.sprite_bounds[index].height << "\n";
    }
    out << "action captures: " << state.captured_pointer_count << "\n";
    out << "action begins: " << state.accepted_begins << "\n";
    out << "action ends: " << state.accepted_ends << "\n";
    out << "action taps: " << state.taps << "\n";
    out << "action cancelled captures: " << state.cancelled_captures << "\n";
    return out.str();
}

}  // namespace nevergone::game_scene_action_surface_input
