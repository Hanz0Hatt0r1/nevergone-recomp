#pragma once

#include <array>
#include <optional>

#include "game_scene_action_input.h"

namespace nevergone::game_scene_action_layout {

struct ButtonDescriptor {
    game_scene_action_input::Control control = game_scene_action_input::Control::kNone;
    int original_tag = 0;
    const char* normal_resource = "";
    const char* selected_resource = "";
    float right_inset = 0.0f;
    float center_y = 0.0f;
};

struct SpriteBounds {
    float width = 0.0f;
    float height = 0.0f;
};

struct Rect {
    float left = 0.0f;
    float bottom = 0.0f;
    float right = 0.0f;
    float top = 0.0f;
};

constexpr int kButtonCount = 3;

// Controls::init() creates the shipped action buttons in this order:
//   ab.png -> tag 1 -> (visible_width - 238, 90)
//   db.png -> tag 2 -> (visible_width - 96, 90)
//   jb.png -> tag 3 -> (visible_width - 136, 224)
// The corresponding selected sprites are *_s.png.
const std::array<ButtonDescriptor, kButtonCount>& descriptors();

std::optional<ButtonDescriptor> descriptor_for(game_scene_action_input::Control control);

// Mirrors ControlsButton::boundingBox() for an unscaled sprite whose anchor is
// the Cocos default center. Actual width/height must come from the decoded
// original resource rather than invented constants.
std::optional<Rect> bounds_for(
    game_scene_action_input::Control control,
    float visible_width,
    SpriteBounds sprite_bounds);

// ControlsButton::touchBegin/touchMoved/touchEnd reject action-button touches
// at or left of half the visible width before checking the normal sprite's
// boundingBox(). Overlapping buttons are resolved in original construction
// order A, B, C.
game_scene_action_input::Control hit_test(
    float visible_width,
    float x,
    float y,
    const std::array<SpriteBounds, kButtonCount>& sprite_bounds);

}  // namespace nevergone::game_scene_action_layout
