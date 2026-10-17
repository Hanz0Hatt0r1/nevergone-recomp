#include "game_scene_action_surface_input.h"

#include <algorithm>
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
constexpr float kDesignHeight = 640.0f;
constexpr int kFirstQuickUseEditTarget = game_scene_action_layout::kButtonCount;
constexpr int kJoystickEditTarget = game_scene_action_layout::kButtonCount +
                                    game_scene_action_layout::kQuickUseButtonCount;

std::mutex g_mutex;
Snapshot g_snapshot;
std::unordered_map<int, game_scene_action_input::Control> g_captures;
bool g_edit_mode = false;
int g_edit_pointer = -1;
int g_edit_target = -1;
float g_edit_start_x = 0.0f;
float g_edit_start_y = 0.0f;
game_scene_action_layout::ControlPositions g_edit_start_positions =
    game_scene_action_layout::default_positions(kDesignWidth);

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

bool contains(const game_scene_action_layout::Rect& rect, float x, float y) {
    return x >= rect.left && x <= rect.right && y >= rect.bottom && y <= rect.top;
}

int edit_target_locked(float x, float y) {
    for (std::size_t index = 0; index < game_scene_action_layout::kButtonCount; ++index) {
        const auto rect = game_scene_action_layout::bounds_for(
            game_scene_action_layout::descriptors()[index].control,
            g_snapshot.positions.buttons[index],
            g_snapshot.sprite_bounds[index]);
        if (rect.has_value() && contains(*rect, x, y)) return static_cast<int>(index);
    }
    for (std::size_t index = 0;
         index < game_scene_action_layout::kQuickUseButtonCount;
         ++index) {
        const auto center = g_snapshot.positions.quick_use_buttons[index];
        const auto& bounds = g_snapshot.quick_use_sprite_bounds[index];
        const auto rect = bounds.width > 0.0f && bounds.height > 0.0f
            ? std::optional<game_scene_action_layout::Rect>(game_scene_action_layout::Rect{
                  center.x - bounds.width * 0.5f,
                  center.y - bounds.height * 0.5f,
                  center.x + bounds.width * 0.5f,
                  center.y + bounds.height * 0.5f})
            : std::nullopt;
        if (rect.has_value() && contains(*rect, x, y)) {
            return kFirstQuickUseEditTarget + static_cast<int>(index);
        }
    }

    const auto joystick = g_snapshot.positions.joystick;
    const float dx = x - joystick.x;
    const float dy = y - joystick.y;
    return std::sqrt(dx * dx + dy * dy) <
            game_scene_player_joystick_input::kHitRadius
        ? kJoystickEditTarget
        : -1;
}

float clamp_center(float value, float half_size, float extent) {
    if (half_size * 2.0f >= extent) return extent * 0.5f;
    return std::clamp(value, half_size, extent - half_size);
}

float clamp_skill_anchor(float value, float minimum, float maximum) {
    return minimum <= maximum ? std::clamp(value, minimum, maximum)
                              : (minimum + maximum) * 0.5f;
}

game_scene_action_layout::ControlPositions moved_editor_target(
    int target,
    float x,
    float y) {
    auto positions = g_edit_start_positions;
    if (target == kJoystickEditTarget) {
        positions.joystick = {
            clamp_center(x, game_scene_player_joystick_input::kHitRadius, kDesignWidth),
            clamp_center(y, game_scene_player_joystick_input::kHitRadius, kDesignHeight),
        };
        return positions;
    }
    if (target >= kFirstQuickUseEditTarget && target < kJoystickEditTarget) {
        const auto slot = static_cast<std::size_t>(target - kFirstQuickUseEditTarget);
        const auto& bounds = g_snapshot.quick_use_sprite_bounds[slot];
        positions.quick_use_buttons[slot] = {
            clamp_center(x, bounds.width * 0.5f, kDesignWidth),
            clamp_center(y, bounds.height * 0.5f, kDesignHeight),
        };
        return positions;
    }

    auto center = positions.buttons[static_cast<std::size_t>(target)];
    const auto& bounds = g_snapshot.sprite_bounds[static_cast<std::size_t>(target)];
    const float half_width = bounds.width * 0.5f;
    const float half_height = bounds.height * 0.5f;
    center.x = clamp_center(x, half_width, kDesignWidth);
    center.y = clamp_center(y, half_height, kDesignHeight);
    if (target == 1) {
        positions.buttons[1] = center;
    } else if (target == 0) {
        positions.unique_skill_tip = {center.x + 51.0f, center.y + 64.0f};
    } else if (target == 2) {
        positions.unique_skill_tip = {center.x - 51.0f, center.y - 64.0f};
    }
    const auto& button_a_bounds = g_snapshot.sprite_bounds[0];
    const auto& button_c_bounds = g_snapshot.sprite_bounds[2];
    positions.unique_skill_tip.x = clamp_skill_anchor(
        positions.unique_skill_tip.x,
        51.0f + button_a_bounds.width * 0.5f,
        kDesignWidth - 51.0f - button_c_bounds.width * 0.5f);
    positions.unique_skill_tip.y = clamp_skill_anchor(
        positions.unique_skill_tip.y,
        64.0f + button_a_bounds.height * 0.5f,
        kDesignHeight - 64.0f - button_c_bounds.height * 0.5f);
    positions.buttons[0] = {
        positions.unique_skill_tip.x - 51.0f,
        positions.unique_skill_tip.y - 64.0f,
    };
    positions.buttons[2] = {
        positions.unique_skill_tip.x + 51.0f,
        positions.unique_skill_tip.y + 64.0f,
    };
    return positions;
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

bool on_edit_touch(
    int action,
    int pointer_id,
    bool mapped,
    float design_x,
    float design_y) {
    game_scene_action_layout::ControlPositions updated_positions;
    bool changed = false;
    {
        std::lock_guard<std::mutex> lock(g_mutex);
        if (!g_edit_mode) return false;
        if (action == kActionCancel) {
            g_edit_pointer = -1;
            g_edit_target = -1;
            g_snapshot.pressed = {};
            return true;
        }
        if (action == kActionDown || action == kActionPointerDown) {
            if (g_edit_pointer != -1 || !mapped) return true;
            g_edit_target = edit_target_locked(design_x, design_y);
            if (g_edit_target < 0) return true;
            g_edit_pointer = pointer_id;
            g_edit_start_x = design_x;
            g_edit_start_y = design_y;
            g_edit_start_positions = g_snapshot.positions;
            if (g_edit_target < game_scene_action_layout::kButtonCount) {
                g_snapshot.pressed[static_cast<std::size_t>(g_edit_target)] = true;
            }
            return true;
        }
        if (g_edit_pointer != pointer_id) return true;
        if (action == kActionMove) {
            if (mapped) {
                const auto initial = g_edit_target == kJoystickEditTarget
                    ? g_edit_start_positions.joystick
                    : g_edit_target >= kFirstQuickUseEditTarget
                        ? g_edit_start_positions.quick_use_buttons[
                              static_cast<std::size_t>(g_edit_target - kFirstQuickUseEditTarget)]
                        : g_edit_start_positions.buttons[static_cast<std::size_t>(g_edit_target)];
                updated_positions = moved_editor_target(
                    g_edit_target,
                    initial.x + design_x - g_edit_start_x,
                    initial.y + design_y - g_edit_start_y);
                g_snapshot.positions = updated_positions;
                changed = true;
            }
        } else if (action == kActionUp || action == kActionPointerUp) {
            g_edit_pointer = -1;
            g_edit_target = -1;
            g_snapshot.pressed = {};
        }
    }
    if (changed) {
        (void)apply_saved_control_settings(
            true,
            updated_positions.joystick,
            updated_positions.buttons[1],
            updated_positions.unique_skill_tip,
            updated_positions.quick_use_buttons);
        (void)game_scene_player_joystick_input::set_center(
            updated_positions.joystick.x,
            updated_positions.joystick.y);
    }
    return true;
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
        g_edit_mode = false;
        g_edit_pointer = -1;
        g_edit_target = -1;
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

bool set_quick_use_sprite_bounds(int slot, int width, int height) {
    if (slot < 0 || slot >= game_scene_action_layout::kQuickUseButtonCount ||
        width <= 0 || height <= 0 || width > 4096 || height > 4096) {
        return false;
    }
    std::lock_guard<std::mutex> lock(g_mutex);
    auto& bounds = g_snapshot.quick_use_sprite_bounds[static_cast<std::size_t>(slot)];
    bounds.width = static_cast<float>(width);
    bounds.height = static_cast<float>(height);
    ++g_snapshot.bounds_revision;
    return true;
}

void clear_quick_use_sprite_bounds() {
    std::lock_guard<std::mutex> lock(g_mutex);
    g_snapshot.quick_use_sprite_bounds = {};
    ++g_snapshot.bounds_revision;
}

bool apply_saved_control_settings(
    bool enabled,
    game_scene_action_layout::Point joystick,
    game_scene_action_layout::Point button_b,
    game_scene_action_layout::Point unique_skill_tip,
    std::array<game_scene_action_layout::Point,
               game_scene_action_layout::kQuickUseButtonCount> quick_use_buttons) {
    if (enabled && (!valid_point(joystick) || !valid_point(button_b) ||
                    !valid_point(unique_skill_tip) ||
                    !valid_point(quick_use_buttons[0]) ||
                    !valid_point(quick_use_buttons[1]))) {
        return false;
    }
    std::lock_guard<std::mutex> lock(g_mutex);
    g_snapshot.positions = game_scene_action_layout::positions_from_saved_settings(
        kDesignWidth,
        enabled,
        joystick,
        button_b,
        unique_skill_tip,
        quick_use_buttons);
    return true;
}

void set_edit_mode(bool enabled) {
    std::vector<game_scene_action_input::Control> released;
    {
        std::lock_guard<std::mutex> lock(g_mutex);
        if (g_edit_mode == enabled) return;
        if (enabled) {
            released.reserve(g_captures.size());
            for (const auto& entry : g_captures) released.push_back(entry.second);
            g_captures.clear();
            g_snapshot.captured_pointer_count = 0;
        }
        g_edit_mode = enabled;
        g_snapshot.edit_mode = enabled;
        g_edit_pointer = -1;
        g_edit_target = -1;
        g_snapshot.pressed = {};
    }
    for (const auto control : released) publish_release(control, false);
    if (enabled) game_scene_player_joystick_input::cancel_capture();
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

    bool editing = false;
    {
        std::lock_guard<std::mutex> lock(g_mutex);
        editing = g_edit_mode;
    }
    if (editing) return on_edit_touch(action, pointer_id, mapped, design_x, design_y);

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
    for (std::size_t index = 0; index < state.quick_use_sprite_bounds.size(); ++index) {
        out << "quick-use sprite " << index << ": "
            << state.quick_use_sprite_bounds[index].width << "x"
            << state.quick_use_sprite_bounds[index].height << "\n";
    }
    out << "action captures: " << state.captured_pointer_count << "\n";
    out << "action begins: " << state.accepted_begins << "\n";
    out << "action ends: " << state.accepted_ends << "\n";
    out << "action taps: " << state.taps << "\n";
    out << "action cancelled captures: " << state.cancelled_captures << "\n";
    return out.str();
}

}  // namespace nevergone::game_scene_action_surface_input
