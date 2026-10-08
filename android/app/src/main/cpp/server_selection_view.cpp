#include "server_selection_view.h"

#include <algorithm>
#include <cmath>

namespace nevergone::server_selection_view {
namespace {

bool finite(float value) {
    return std::isfinite(value);
}

}  // namespace

SurfaceMapping mapping_for_surface(int surface_width, int surface_height) {
    SurfaceMapping result;
    if (surface_width <= 0 || surface_height <= 0) return result;

    const float width = static_cast<float>(surface_width);
    const float height = static_cast<float>(surface_height);
    const float scale = std::min(
        width / server_selection_layout::kDesignWidth,
        height / server_selection_layout::kDesignHeight);
    if (!finite(scale) || scale <= 0.0f) return result;

    result.valid = true;
    result.scale = scale;
    result.viewport_width = server_selection_layout::kDesignWidth * scale;
    result.viewport_height = server_selection_layout::kDesignHeight * scale;
    result.offset_x = (width - result.viewport_width) * 0.5f;
    result.offset_y = (height - result.viewport_height) * 0.5f;
    return result;
}

Point surface_to_design(
    int surface_width,
    int surface_height,
    float surface_x,
    float surface_y) {
    Point result;
    if (!finite(surface_x) || !finite(surface_y)) return result;
    const SurfaceMapping mapping = mapping_for_surface(surface_width, surface_height);
    if (!mapping.valid) return result;

    const float local_x = surface_x - mapping.offset_x;
    const float local_y_from_top = surface_y - mapping.offset_y;
    if (local_x < 0.0f || local_x > mapping.viewport_width ||
            local_y_from_top < 0.0f || local_y_from_top > mapping.viewport_height) {
        return result;
    }

    result.inside_design = true;
    result.x = local_x / mapping.scale;
    result.y = server_selection_layout::kDesignHeight -
        (local_y_from_top / mapping.scale);
    return result;
}

server_selection_layout::RowRect confirm_rect() {
    server_selection_layout::RowRect result;
    result.index = 0;
    result.original_tag = 10002;
    result.left = kFallbackConfirmLeft;
    result.bottom = kFallbackConfirmBottom;
    result.width = kFallbackConfirmWidth;
    result.height = kFallbackConfirmHeight;
    return result;
}

bool confirm_contains(float design_x, float design_y) {
    return server_selection_layout::contains(confirm_rect(), design_x, design_y);
}

int hit_test_surface(
    std::size_t server_count,
    int surface_width,
    int surface_height,
    float surface_x,
    float surface_y,
    float row_width,
    float row_height,
    float scroll_offset_y) {
    const Point point = surface_to_design(
        surface_width, surface_height, surface_x, surface_y);
    if (!point.inside_design) return -1;
    return server_selection_layout::hit_test(
        server_count,
        row_width,
        row_height,
        point.x,
        point.y,
        scroll_offset_y);
}

}  // namespace nevergone::server_selection_view
