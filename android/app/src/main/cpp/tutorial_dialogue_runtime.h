#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

// The JNI bridge is emitted from the same translation unit outside the runtime
// namespace. Keep the identical hard array bound visible to that bridge.
inline constexpr std::size_t kMaxPixels = 16u * 1024u * 1024u;

namespace nevergone::tutorial_dialogue_runtime {

enum class Stage : int {
    kInactive = 0,
    kFirstDialogue = 1,
    kLessonInterlude = 2,
    kSecondDialogue = 3,
    kComplete = 4,
};

struct Snapshot {
    Stage stage = Stage::kInactive;
    std::uint64_t generation = 0;
    int dialogue_id = 0;
    int line_index = 0;
    int line_count = 0;
    int visible_text_characters = 0;
    int current_text_character_count = 0;
    bool assets_ready = false;
    bool auto_play = false;
    double fade_in_seconds = 0.0;
    double fade_out_seconds = 0.0;
    std::uint64_t line_started_tick = 0;
    std::uint64_t first_dialogue_completion_count = 0;
    std::uint64_t draw_attempt_count = 0;
    std::uint64_t drawn_frame_count = 0;
};

void reset();

// Exact LessonLayer::menuContinueCallback -> firstDlgCallBack boundary.
// The original requests Guide/guide_text.plist, dialogue id 8, callback tag 1.
bool begin_first_dialogue();

// Advances only from timing loaded out of the user's imported guide plist.
// The final ID-8 line enters kLessonInterlude. The Android frame boundary then
// mirrors dialogEndCallBack(1) -> showDialogue() by advancing the coarse
// tutorial session into Interactive. Dialogue id 9 remains a later, separate
// boundary: the original schedules secondDlgCallBack after lesson step 12 sees
// BattleManager::enemyNullCount()==0.
void tick();

void on_surface_created();
void on_surface_changed(int width, int height);
bool draw();

// Transactional asset/config upload used by TutorialDialogueAssetLoader.
void clear_assets();
bool begin_assets(
    int dialogue_id,
    bool auto_play,
    double fade_in_seconds,
    double fade_out_seconds,
    int line_count);
bool upload_background(
    int width,
    int height,
    const std::uint32_t* argb,
    std::size_t count);
bool upload_line(
    int line_index,
    int npc,
    int npc_position,
    double appear_seconds,
    double show_delay_seconds,
    bool fade_in,
    bool fade_out,
    int portrait_width,
    int portrait_height,
    const std::uint32_t* portrait_argb,
    std::size_t portrait_count,
    int name_width,
    int name_height,
    const std::uint32_t* name_argb,
    std::size_t name_count,
    int text_width,
        int text_height,
        const std::uint32_t* text_argb,
        std::size_t text_count,
        int text_character_count);
bool finish_assets();

Snapshot snapshot();
const char* stage_name(Stage stage);
std::string status_report();

}  // namespace nevergone::tutorial_dialogue_runtime
