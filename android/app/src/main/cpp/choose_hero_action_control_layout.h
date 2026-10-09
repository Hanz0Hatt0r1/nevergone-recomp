#pragma once

namespace nevergone::choose_hero_action_control_layout {

constexpr float kRightEdgeFraction = 0.9f;
constexpr float kBottomGap = 5.0f;
constexpr float kDeleteGap = 10.0f;
constexpr float kType1LabelMaxWidth = 115.0f;

struct ButtonPose {
    float center_x = 0.0f;
    float center_y = 0.0f;
    float width = 0.0f;
    float height = 0.0f;
};

struct Layout {
    bool valid = false;
    ButtonPose play;
    ButtonPose delete_hero;
};

// Reconstructs ChooseHero::HeroInformation placement for the two type-1
// MRFixedButtons. The original uses the Play button content size for both
// vertical placement and the Delete button's horizontal offset.
Layout compute(
    float visible_width,
    float play_button_width,
    float play_button_height,
    float delete_button_width,
    float delete_button_height);

// SetMRFixedButtonLableSize(type=1) delegates to SetMRLableSizeX(label,115).
// The helper only shrinks; it never enlarges a shorter label.
float type1_label_scale(float label_width);

bool contains(const ButtonPose& pose, float x, float y);

}  // namespace nevergone::choose_hero_action_control_layout
