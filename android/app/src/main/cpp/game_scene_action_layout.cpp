#include "game_scene_action_layout.h"

#include <cmath>

namespace nevergone::game_scene_action_layout {
namespace {

constexpr std::array<ButtonDescriptor, kButtonCount> kDescriptors{{
    {game_scene_action_input::Control::kButtonA, 1, "ab.png", "ab_s.png", 238.0f, 90.0f},
    {game_scene_action_input::Control::kButtonB, 2, "db.png", "db_s.png", 96.0f, 90.0f},
    {game_scene_action_input::Control::kButtonC, 3, "jb.png", "jb_s.png", 136.0f, 224.0f},
}};

constexpr float kJoystickX = 204.0f;
constexpr float kJoystickY = 173.0f;
constexpr float kDesignHeight = 640.0f;
constexpr float kSkillToButtonOffsetX = 51.0f;
constexpr float kSkillToButtonOffsetY = 64.0f;

bool contains(const Rect& rect, float x, float y) {
    return x >= rect.left && x <= rect.right && y >= rect.bottom && y <= rect.top;
}

}  // namespace

const std::array<ButtonDescriptor, kButtonCount>& descriptors() {
    return kDescriptors;
}

ControlPositions default_positions(float visible_width) {
    ControlPositions positions;
    positions.joystick = {kJoystickX, kJoystickY};
    for (std::size_t index = 0; index < kDescriptors.size(); ++index) {
        positions.buttons[index] = {
            visible_width - kDescriptors[index].right_inset,
            kDescriptors[index].center_y,
        };
    }
    positions.unique_skill_tip = {visible_width - 187.0f, 154.0f};
    positions.quick_use_buttons = {{{visible_width, kDesignHeight},
                                    {visible_width, kDesignHeight}}};
    return positions;
}

ControlPositions positions_from_saved_settings(
    float visible_width,
    bool controls_settings,
    Point joystick,
    Point button_b,
    Point unique_skill_tip,
    std::array<Point, kQuickUseButtonCount> quick_use_buttons) {
    ControlPositions positions = default_positions(visible_width);
    if (!controls_settings) return positions;

    positions.joystick = joystick;
    positions.buttons[1] = button_b;
    positions.unique_skill_tip = unique_skill_tip;
    positions.quick_use_buttons = quick_use_buttons;
    positions.buttons[0] = {
        unique_skill_tip.x - kSkillToButtonOffsetX,
        unique_skill_tip.y - kSkillToButtonOffsetY,
    };
    positions.buttons[2] = {
        unique_skill_tip.x + kSkillToButtonOffsetX,
        unique_skill_tip.y + kSkillToButtonOffsetY,
    };
    return positions;
}

std::optional<ButtonDescriptor> descriptor_for(game_scene_action_input::Control control) {
    for (const auto& descriptor : kDescriptors) {
        if (descriptor.control == control) return descriptor;
    }
    return std::nullopt;
}

std::optional<Rect> bounds_for(
    game_scene_action_input::Control control,
    float visible_width,
    SpriteBounds sprite_bounds) {
    if (visible_width <= 0.0f || sprite_bounds.width <= 0.0f || sprite_bounds.height <= 0.0f) {
        return std::nullopt;
    }
    const auto descriptor = descriptor_for(control);
    if (!descriptor.has_value()) return std::nullopt;

    const float center_x = visible_width - descriptor->right_inset;
    return bounds_for(control, {center_x, descriptor->center_y}, sprite_bounds);
}

std::optional<Rect> bounds_for(
    game_scene_action_input::Control control,
    Point center,
    SpriteBounds sprite_bounds) {
    if (!std::isfinite(center.x) || !std::isfinite(center.y) ||
        sprite_bounds.width <= 0.0f || sprite_bounds.height <= 0.0f) {
        return std::nullopt;
    }
    if (!descriptor_for(control).has_value()) return std::nullopt;

    const float half_width = sprite_bounds.width * 0.5f;
    const float half_height = sprite_bounds.height * 0.5f;
    return Rect{
        center.x - half_width,
        center.y - half_height,
        center.x + half_width,
        center.y + half_height,
    };
}

game_scene_action_input::Control hit_test(
    float visible_width,
    float x,
    float y,
    const std::array<SpriteBounds, kButtonCount>& sprite_bounds) {
    return hit_test(
        visible_width,
        x,
        y,
        sprite_bounds,
        default_positions(visible_width));
}

game_scene_action_input::Control hit_test(
    float visible_width,
    float x,
    float y,
    const std::array<SpriteBounds, kButtonCount>& sprite_bounds,
    const ControlPositions& positions) {
    if (visible_width <= 0.0f || x <= visible_width * 0.5f) {
        return game_scene_action_input::Control::kNone;
    }

    for (std::size_t index = 0; index < kDescriptors.size(); ++index) {
        const auto rect = bounds_for(
            kDescriptors[index].control,
            positions.buttons[index],
            sprite_bounds[index]);
        if (rect.has_value() && contains(*rect, x, y)) {
            return kDescriptors[index].control;
        }
    }
    return game_scene_action_input::Control::kNone;
}

}  // namespace nevergone::game_scene_action_layout
