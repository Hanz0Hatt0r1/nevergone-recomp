#pragma once

namespace nevergone::choose_hero_role_item_view {

constexpr float kDesignWidth = 1136.0f;
constexpr float kDesignHeight = 640.0f;

struct Mapping {
    bool valid = false;
    float scale = 0.0f;
    float offset_x = 0.0f;
    float offset_y = 0.0f;
    float viewport_width = 0.0f;
    float viewport_height = 0.0f;
};

struct Point {
    bool inside = false;
    float x = 0.0f;
    float y = 0.0f;
};

Mapping mapping_for_surface(int surface_width, int surface_height);
Point surface_to_design(
    int surface_width,
    int surface_height,
    float surface_x,
    float surface_y);
bool contains_centered(
    float center_x,
    float center_y,
    float width,
    float height,
    float x,
    float y);

}  // namespace nevergone::choose_hero_role_item_view
