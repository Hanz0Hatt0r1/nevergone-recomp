#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

namespace nevergone::tutorial_lesson_overlay {

void on_surface_created();
void on_surface_changed(int width, int height);

void clear_assets();
bool upload_keyboard(
    int width,
    int height,
    const std::uint32_t* argb,
    std::size_t count);
bool upload_continue_label(
    int width,
    int height,
    const std::uint32_t* argb,
    std::size_t count);

// Draws only while tutorial_session_state owns LessonOverlay.
// Returns true when both imported assets were ready and submitted.
bool draw();

// Consumes the modal LessonOverlay surface. Only the recovered Continue center
// is actionable; taps elsewhere are deliberately swallowed so gameplay input
// cannot leak through the original modal tutorial boundary.
bool on_touch(int action, int pointer_id, float surface_x, float surface_y);

std::string status_report();

}  // namespace nevergone::tutorial_lesson_overlay
