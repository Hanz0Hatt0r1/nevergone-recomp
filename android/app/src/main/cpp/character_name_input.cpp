#include "character_name_input.h"

#include <algorithm>
#include <cmath>
#include <mutex>

#include "character_name_assets.h"
#include "character_name_layout.h"

namespace nevergone::character_name_input {
namespace {

constexpr float kDesignWidth = 1136.0f;
constexpr float kDesignHeight = 640.0f;
constexpr int kConfirmTag = 1;
constexpr int kCancelTag = 2;
constexpr int kRandomTag = 3;

std::mutex g_mutex;
int g_pointer_id = -1;
int g_armed_tag = 0;
int g_pressed_tag = 0;

void clear_locked() {
    g_pointer_id = -1;
    g_armed_tag = 0;
    g_pressed_tag = 0;
}

bool surface_to_design(
        float surface_x,
        float surface_y,
        int surface_width,
        int surface_height,
        float* design_x,
        float* design_y) {
    if (surface_width <= 0 || surface_height <= 0 || design_x == nullptr ||
            design_y == nullptr || !std::isfinite(surface_x) ||
            !std::isfinite(surface_y)) {
        return false;
    }

    const float scale = std::min(
        static_cast<float>(surface_width) / kDesignWidth,
        static_cast<float>(surface_height) / kDesignHeight);
    if (!std::isfinite(scale) || scale <= 0.0f) return false;

    const float offset_x =
        (static_cast<float>(surface_width) - kDesignWidth * scale) * 0.5f;
    const float offset_y =
        (static_cast<float>(surface_height) - kDesignHeight * scale) * 0.5f;
    *design_x = (surface_x - offset_x) / scale;
    const float top_origin_y = (surface_y - offset_y) / scale;
    *design_y = kDesignHeight - top_origin_y;
    return std::isfinite(*design_x) && std::isfinite(*design_y);
}

character_name_layout::Layout current_layout(
        const character_name_assets::Snapshot& assets) {
    if (!assets.ready) return {};

    const auto& background = assets.images[
        static_cast<std::size_t>(character_name_assets::ImageSlot::kBackground)];
    const auto& name_plate = assets.images[
        static_cast<std::size_t>(character_name_assets::ImageSlot::kNamePlate)];
    const auto& random_button = assets.images[
        static_cast<std::size_t>(character_name_assets::ImageSlot::kRandomButton)];
    const auto& fixed_button = assets.images[
        static_cast<std::size_t>(character_name_assets::ImageSlot::kFixedButtonNormal)];

    return character_name_layout::compute(
        kDesignWidth,
        kDesignHeight,
        static_cast<float>(background.width),
        static_cast<float>(background.height),
        static_cast<float>(name_plate.width),
        static_cast<float>(name_plate.height),
        static_cast<float>(random_button.width),
        static_cast<float>(random_button.height),
        static_cast<float>(fixed_button.width),
        static_cast<float>(fixed_button.height),
        static_cast<float>(fixed_button.width),
        static_cast<float>(fixed_button.height));
}

int hit_tag(
        const character_name_layout::Layout& layout,
        float design_x,
        float design_y) {
    if (!layout.valid) return 0;
    if (character_name_layout::contains(layout.random_button, design_x, design_y)) {
        return kRandomTag;
    }
    if (character_name_layout::contains(layout.confirm_button, design_x, design_y)) {
        return kConfirmTag;
    }
    if (character_name_layout::contains(layout.cancel_button, design_x, design_y)) {
        return kCancelTag;
    }
    return 0;
}

}  // namespace

bool on_touch_for_surface(
        int action,
        int pointer_id,
        float x,
        float y,
        int surface_width,
        int surface_height,
        bool active,
        DispatchFn dispatch) {
    if (!active) {
        reset();
        return false;
    }

    if (surface_width <= 0 || surface_height <= 0 || !std::isfinite(x) ||
            !std::isfinite(y)) {
        reset();
        return true;
    }

    const auto assets = character_name_assets::snapshot();
    if (!assets.ready) {
        reset();
        // CharacterName is modal even if the optional native presentation
        // cannot be staged; keep touches from falling through to older layers.
        return true;
    }

    const auto layout = current_layout(assets);
    float design_x = 0.0f;
    float design_y = 0.0f;
    const bool mapped = surface_to_design(
        x, y, surface_width, surface_height, &design_x, &design_y);
    const int current_tag = mapped ? hit_tag(layout, design_x, design_y) : 0;

    int dispatch_tag = 0;
    {
        std::lock_guard<std::mutex> lock(g_mutex);
        switch (action) {
            case 0:  // ACTION_DOWN
            case 5:  // ACTION_POINTER_DOWN
                if (g_pointer_id != -1) return true;
                g_pointer_id = pointer_id;
                g_armed_tag = current_tag;
                g_pressed_tag = current_tag;
                return true;

            case 2:  // ACTION_MOVE
                if (g_pointer_id == -1 || pointer_id != g_pointer_id) return true;
                g_pressed_tag =
                    g_armed_tag != 0 && current_tag == g_armed_tag ? g_armed_tag : 0;
                return true;

            case 1:  // ACTION_UP
            case 6:  // ACTION_POINTER_UP
                if (g_pointer_id == -1 || pointer_id != g_pointer_id) return true;
                if (g_armed_tag != 0 && current_tag == g_armed_tag) {
                    dispatch_tag = g_armed_tag;
                }
                clear_locked();
                break;

            case 3:  // ACTION_CANCEL
                clear_locked();
                return true;

            default:
                return true;
        }
    }

    if (dispatch_tag != 0 && dispatch != nullptr) dispatch(dispatch_tag);
    return true;
}

int pressed_tag() {
    std::lock_guard<std::mutex> lock(g_mutex);
    return g_pressed_tag;
}

void reset() {
    std::lock_guard<std::mutex> lock(g_mutex);
    clear_locked();
}

}  // namespace nevergone::character_name_input
