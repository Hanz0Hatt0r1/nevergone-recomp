#pragma once

#include <cstddef>

#include "server_selection_layout.h"

namespace nevergone::server_selection_view {

// Project-owned fallback dimensions used only when user-imported expansion
// assets do not provide the recovered NewServerList artwork. The recovered
// confirm center is independent of those fallback dimensions.
constexpr float kFallbackRowWidth = 440.0f;
constexpr float kFallbackRowHeight = 72.0f;
constexpr float kConfirmCenterX = 568.0f;
constexpr float kConfirmCenterY = 100.0f;
constexpr float kFallbackConfirmWidth = 180.0f;
constexpr float kFallbackConfirmHeight = 64.0f;

struct Point {
    bool inside_design = false;
    float x = 0.0f;
    float y = 0.0f;
};

struct SurfaceMapping {
    bool valid = false;
    float scale = 0.0f;
    float offset_x = 0.0f;
    float offset_y = 0.0f;
    float viewport_width = 0.0f;
    float viewport_height = 0.0f;
};

SurfaceMapping mapping_for_surface(int surface_width, int surface_height);
Point surface_to_design(
    int surface_width,
    int surface_height,
    float surface_x,
    float surface_y);

server_selection_layout::RowRect confirm_rect(
    float width = kFallbackConfirmWidth,
    float height = kFallbackConfirmHeight);
bool confirm_contains(
    float design_x,
    float design_y,
    float width = kFallbackConfirmWidth,
    float height = kFallbackConfirmHeight);

int hit_test_surface(
    std::size_t server_count,
    int surface_width,
    int surface_height,
    float surface_x,
    float surface_y,
    float row_width = kFallbackRowWidth,
    float row_height = kFallbackRowHeight,
    float scroll_offset_y = 0.0f);

}  // namespace nevergone::server_selection_view
