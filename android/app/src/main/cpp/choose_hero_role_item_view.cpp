#include "choose_hero_role_item_view.h"

#include <algorithm>
#include <cmath>

namespace nevergone::choose_hero_role_item_view {

Mapping mapping_for_surface(int surface_width, int surface_height) {
    Mapping result;
    if (surface_width <= 0 || surface_height <= 0) return result;
    const float width = static_cast<float>(surface_width);
    const float height = static_cast<float>(surface_height);
    const float scale = std::min(width / kDesignWidth, height / kDesignHeight);
    if (!std::isfinite(scale) || scale <= 0.0f) return result;

    result.valid = true;
    result.scale = scale;
    result.viewport_width = kDesignWidth * scale;
    result.viewport_height = kDesignHeight * scale;
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
    if (!std::isfinite(surface_x) || !std::isfinite(surface_y)) return result;
    const Mapping mapping = mapping_for_surface(surface_width, surface_height);
    if (!mapping.valid) return result;

    const float local_x = surface_x - mapping.offset_x;
    const float local_y_from_top = surface_y - mapping.offset_y;
    if (local_x < 0.0f || local_x > mapping.viewport_width ||
            local_y_from_top < 0.0f || local_y_from_top > mapping.viewport_height) {
        return result;
    }

    result.inside = true;
    result.x = local_x / mapping.scale;
    result.y = kDesignHeight - local_y_from_top / mapping.scale;
    return result;
}

bool contains_centered(
        float center_x,
        float center_y,
        float width,
        float height,
        float x,
        float y) {
    if (!std::isfinite(center_x) || !std::isfinite(center_y) ||
            !std::isfinite(width) || !std::isfinite(height) ||
            !std::isfinite(x) || !std::isfinite(y) ||
            width <= 0.0f || height <= 0.0f) {
        return false;
    }
    const float half_width = width * 0.5f;
    const float half_height = height * 0.5f;
    return x >= center_x - half_width && x <= center_x + half_width &&
        y >= center_y - half_height && y <= center_y + half_height;
}

}  // namespace nevergone::choose_hero_role_item_view
