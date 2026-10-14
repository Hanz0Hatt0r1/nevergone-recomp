#pragma once

#include <array>
#include <cstddef>

namespace nevergone::tutorial_lesson_overlay_layout {

constexpr float kDesignWidth = 1136.0f;
constexpr float kDesignHeight = 640.0f;
constexpr float kKeyboardCenterX = kDesignWidth * 0.5f;
constexpr float kKeyboardCenterY = kDesignHeight * 0.5f;
constexpr float kContinueCenterX = kDesignWidth - 160.0f;
constexpr float kContinueCenterY = 45.0f;
constexpr float kContinueLabelMaxWidth = 115.0f;
constexpr float kContinueFontSize = 25.0f;
constexpr float kInfoFontSize = 18.0f;
constexpr float kInfoTint = 196.0f / 255.0f;

enum class Label : int {
    kContinue = 0,
    kHeroXp,
    kHealth,
    kRage,
    kBladeHeat,
    kStamina,
    kDirection,
    kBossHp,
    kOptionPause,
    kMonsterHp,
    kJump,
    kBlock,
    kAttack,
    kItem,
    kBodyArmor,
    kCount,
};

struct LabelPose {
    Label label = Label::kContinue;
    float x = 0.0f;
    float y = 0.0f;
    float anchor_x = 0.5f;
    float anchor_y = 0.5f;
};

struct ButtonPose {
    bool valid = false;
    float center_x = kContinueCenterX;
    float center_y = kContinueCenterY;
    float width = 0.0f;
    float height = 0.0f;
};

// Recovered from LessonLayer::showKeyboardPic(). The initial keyboard sprite is
// centered on the visible surface. The informational CCLabelTTFs are Arial 18
// tinted RGB(196,196,196); left-column labels use anchor (1,0.5), right-column
// labels use (0,0.5), and Item keeps the default centered anchor.
const std::array<LabelPose, 14>& info_label_poses();

ButtonPose continue_button(float imported_width, float imported_height);
bool contains(const ButtonPose& pose, float x, float y);
float continue_label_scale(float label_width);
const char* localization_key(Label label);

}  // namespace nevergone::tutorial_lesson_overlay_layout
