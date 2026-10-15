#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

namespace nevergone::tutorial_dialogue_overlay {

// The fresh-role LessonLayer enters the shipped Guide/guide_text.plist group 8
// through LessonLayer::firstDlgCallBack().  This compositor owns only that
// startup dialogue boundary; later guide group 9 belongs to the terminal
// tutorial callback and is intentionally not collapsed into this sequence.
constexpr int kStartupGuideDialogueId = 8;

void on_surface_created();
void on_surface_changed(int width, int height);

void clear_assets();
bool configure_sequence(int dialogue_id, bool auto_play, int page_count);
bool upload_background(
    int width,
    int height,
    const std::uint32_t* argb,
    std::size_t count);
bool upload_page(
    int page_index,
    int width,
    int height,
    const std::uint32_t* argb,
    std::size_t count);
bool commit_assets();

// Draws the recovered Dialogue_UI/dialogue_bg.png plus the current imported,
// localized page while tutorial_session_state owns the modal Dialogue phase.
bool draw();

// DialogueUI::ccTouchBegan() accepts the whole surface.  A completed touch is
// the recovered manual nextText() path when auto_play is false.  Finishing
// group 8 publishes the existing tutorial-session dialogue completion event.
bool on_touch(int action, int pointer_id, float surface_x, float surface_y);

std::string status_report();

}  // namespace nevergone::tutorial_dialogue_overlay
