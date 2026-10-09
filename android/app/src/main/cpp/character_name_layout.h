#pragma once

namespace nevergone::character_name_layout {

constexpr float kBackgroundTopOffset = 188.0f;
constexpr float kNameGapBelowBackground = 10.0f;
constexpr float kActionSideInset = 135.0f;
constexpr float kActionCenterY = 52.0f;
constexpr float kRandomButtonGap = 40.0f;
constexpr float kEditBoxHeight = 30.0f;

struct Pose {
    float center_x = 0.0f;
    float center_y = 0.0f;
    float width = 0.0f;
    float height = 0.0f;
};

struct Layout {
    bool valid = false;
    Pose background;
    Pose title;
    Pose name_plate;
    Pose edit_box;
    Pose random_button;
    Pose confirm_button;
    Pose cancel_button;
    Pose touch_blocker;
};

// Clean-room CharacterNameLayer::CretaUI placement recovered from the shipped
// ARMv7 implementation. Resource dimensions remain runtime inputs because the
// original takes them from the imported sprites/menu items.
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
    float cancel_button_height);

bool contains(const Pose& pose, float x, float y);

}  // namespace nevergone::character_name_layout
