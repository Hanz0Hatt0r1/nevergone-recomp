#include "character_name_layout.h"

#include <cmath>

namespace nevergone::character_name_layout {
namespace {

bool positive_finite(float value) {
    return std::isfinite(value) && value > 0.0f;
}

Pose make_pose(float x, float y, float width, float height) {
    Pose pose;
    pose.center_x = x;
    pose.center_y = y;
    pose.width = width;
    pose.height = height;
    return pose;
}

}  // namespace

Layout compute(
        float visible_width,
        float visible_height,
        float background_width,
        float background_height,
        float name_plate_width,
        float name_plate_height,
        float random_button_width,
        float random_button_height,
        float confirm_button_width,
        float confirm_button_height,
        float cancel_button_width,
        float cancel_button_height) {
    Layout layout;
    if (!positive_finite(visible_width) || !positive_finite(visible_height) ||
        !positive_finite(background_width) || !positive_finite(background_height) ||
        !positive_finite(name_plate_width) || !positive_finite(name_plate_height) ||
        !positive_finite(random_button_width) || !positive_finite(random_button_height) ||
        !positive_finite(confirm_button_width) || !positive_finite(confirm_button_height) ||
        !positive_finite(cancel_button_width) || !positive_finite(cancel_button_height)) {
        return layout;
    }

    const float center_x = visible_width * 0.5f;
    const float background_y = visible_height - kBackgroundTopOffset;
    const float name_y = background_y - background_height * 0.5f -
                         kNameGapBelowBackground - name_plate_height * 0.5f;

    layout.background = make_pose(
        center_x, background_y, background_width, background_height);

    // The Prompt9 label is positioned at the Redbottom sprite position.
    // Its intrinsic dimensions are owned by the text renderer, so the layout
    // only needs the recovered center.
    layout.title = make_pose(center_x, background_y, 0.0f, 0.0f);

    layout.name_plate = make_pose(
        center_x, name_y, name_plate_width, name_plate_height);

    // CretaUI creates the transparent scale-9 edit box with the RANDOMName
    // sprite's content width and a literal 30-pixel height, then places it at
    // the same point as the name plate.
    layout.edit_box = make_pose(
        center_x, name_y, name_plate_width, kEditBoxHeight);

    layout.random_button = make_pose(
        center_x + name_plate_width * 0.5f + kRandomButtonGap,
        name_y,
        random_button_width,
        random_button_height);

    layout.confirm_button = make_pose(
        visible_width - kActionSideInset,
        kActionCenterY,
        confirm_button_width,
        confirm_button_height);
    layout.cancel_button = make_pose(
        kActionSideInset,
        kActionCenterY,
        cancel_button_width,
        cancel_button_height);

    // The Button_C_a/Button_C_b menu item is centered and scaled independently
    // in X/Y to the visible size, then made transparent. Its recovered tag 5
    // has no CharacterName action; it acts as the modal touch blocker behind
    // the three actionable controls.
    layout.touch_blocker = make_pose(
        center_x, visible_height * 0.5f, visible_width, visible_height);

    layout.valid = true;
    return layout;
}

bool contains(const Pose& pose, float x, float y) {
    if (!positive_finite(pose.width) || !positive_finite(pose.height) ||
        !std::isfinite(x) || !std::isfinite(y)) {
        return false;
    }
    const float half_width = pose.width * 0.5f;
    const float half_height = pose.height * 0.5f;
    return x >= pose.center_x - half_width && x <= pose.center_x + half_width &&
           y >= pose.center_y - half_height && y <= pose.center_y + half_height;
}

}  // namespace nevergone::character_name_layout
