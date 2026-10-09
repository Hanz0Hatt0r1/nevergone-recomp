#include "choose_hero_action_control_layout.h"

#include <cmath>

namespace nevergone::choose_hero_action_control_layout {

namespace {

bool positive_finite(float value) {
    return std::isfinite(value) && value > 0.0f;
}

}  // namespace

Layout compute(
        float visible_width,
        float play_button_width,
        float play_button_height,
        float delete_button_width,
        float delete_button_height) {
    Layout result;
    if (!positive_finite(visible_width) ||
            !positive_finite(play_button_width) ||
            !positive_finite(play_button_height) ||
            !positive_finite(delete_button_width) ||
            !positive_finite(delete_button_height)) {
        return result;
    }

    // HeroInformation stores CCDirector::getVisibleSize() at ChooseHero+0x13c.
    // The Play center uses width*0.9 and Y=playHeight+5.
    result.play.center_x = visible_width * kRightEdgeFraction;
    result.play.center_y = play_button_height + kBottomGap;
    result.play.width = play_button_width;
    result.play.height = play_button_height;

    // The shipped delete button X subtracts the Play content width and 10 from
    // the same width*0.9 anchor. Its Y also uses Play height + 5.
    result.delete_hero.center_x =
        visible_width * kRightEdgeFraction - play_button_width - kDeleteGap;
    result.delete_hero.center_y = play_button_height + kBottomGap;
    result.delete_hero.width = delete_button_width;
    result.delete_hero.height = delete_button_height;
    result.valid = true;
    return result;
}

float type1_label_scale(float label_width) {
    if (!positive_finite(label_width)) return 0.0f;
    return label_width > kType1LabelMaxWidth
        ? kType1LabelMaxWidth / label_width
        : 1.0f;
}

bool contains(const ButtonPose& pose, float x, float y) {
    if (!positive_finite(pose.width) || !positive_finite(pose.height) ||
            !std::isfinite(pose.center_x) || !std::isfinite(pose.center_y) ||
            !std::isfinite(x) || !std::isfinite(y)) {
        return false;
    }
    return x >= pose.center_x - pose.width * 0.5f &&
        x <= pose.center_x + pose.width * 0.5f &&
        y >= pose.center_y - pose.height * 0.5f &&
        y <= pose.center_y + pose.height * 0.5f;
}

}  // namespace nevergone::choose_hero_action_control_layout
