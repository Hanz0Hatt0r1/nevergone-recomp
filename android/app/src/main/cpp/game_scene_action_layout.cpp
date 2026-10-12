#include "game_scene_action_layout.h"

namespace nevergone::game_scene_action_layout {
namespace {

constexpr std::array<ButtonDescriptor, kButtonCount> kDescriptors{{
    {game_scene_action_input::Control::kButtonA, 1, "ab.png", "ab_s.png", 238.0f, 90.0f},
    {game_scene_action_input::Control::kButtonB, 2, "db.png", "db_s.png", 96.0f, 90.0f},
    {game_scene_action_input::Control::kButtonC, 3, "jb.png", "jb_s.png", 136.0f, 224.0f},
}};

bool contains(const Rect& rect, float x, float y) {
    return x >= rect.left && x <= rect.right && y >= rect.bottom && y <= rect.top;
}

}  // namespace

const std::array<ButtonDescriptor, kButtonCount>& descriptors() {
    return kDescriptors;
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
    const float half_width = sprite_bounds.width * 0.5f;
    const float half_height = sprite_bounds.height * 0.5f;
    return Rect{
        center_x - half_width,
        descriptor->center_y - half_height,
        center_x + half_width,
        descriptor->center_y + half_height,
    };
}

game_scene_action_input::Control hit_test(
    float visible_width,
    float x,
    float y,
    const std::array<SpriteBounds, kButtonCount>& sprite_bounds) {
    if (visible_width <= 0.0f || x <= visible_width * 0.5f) {
        return game_scene_action_input::Control::kNone;
    }

    for (std::size_t index = 0; index < kDescriptors.size(); ++index) {
        const auto rect = bounds_for(kDescriptors[index].control, visible_width, sprite_bounds[index]);
        if (rect.has_value() && contains(*rect, x, y)) {
            return kDescriptors[index].control;
        }
    }
    return game_scene_action_input::Control::kNone;
}

}  // namespace nevergone::game_scene_action_layout
