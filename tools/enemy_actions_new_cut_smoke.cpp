#include <cassert>
#include <limits>

#include "enemy_actions_new_cut.h"

int main() {
    namespace cut = nevergone::enemy_actions_new_cut;

    auto result = cut::materialize(
            true, true, "frame.png", 100.0f, 640.0f, 600.0f);
    assert(result.unmatched_enemy_cut_path);
    assert(result.sprite_frame_resolved);
    assert(result.should_patch_sprite_frame_rect);
    assert(result.should_create_actions_cut);
    assert(result.should_append_to_enemy_cut_array);
    assert(result.frame_name_14 == "frame.png");
    assert(result.delta == 40.0f);
    assert(result.field_18 == 20.0f);
    assert(result.field_1c == 100.0f);
    assert(result.patched_rect_height == 60.0f);

    // Missing spriteFrameByName result prevents the native allocation/append
    // side effects even though the unmatched EnemyObject branch was selected.
    result = cut::materialize(
            true, false, "frame.png", 100.0f, 640.0f, 600.0f);
    assert(result.unmatched_enemy_cut_path);
    assert(!result.sprite_frame_resolved);
    assert(!result.should_patch_sprite_frame_rect);
    assert(!result.should_create_actions_cut);
    assert(!result.should_append_to_enemy_cut_array);
    assert(result.frame_name_14.empty());

    // A call outside the unmatched EnemyObject branch must remain a no-op.
    result = cut::materialize(
            false, true, "frame.png", 100.0f, 640.0f, 600.0f);
    assert(!result.should_create_actions_cut);
    assert(!result.should_append_to_enemy_cut_array);

    // Equality leaves delta zero but still materializes the cut when the path
    // and sprite frame are valid.
    result = cut::materialize(
            true, true, "equal.png", 80.0f, 640.0f, 640.0f);
    assert(result.should_create_actions_cut);
    assert(result.delta == 0.0f);
    assert(result.field_18 == 0.0f);
    assert(result.field_1c == 80.0f);
    assert(result.patched_rect_height == 80.0f);

    // Unordered H-point input inherits the recovered zero-delta semantics.
    const float nan = std::numeric_limits<float>::quiet_NaN();
    result = cut::materialize(
            true, true, "nan.png", 50.0f, 640.0f, nan);
    assert(result.should_create_actions_cut);
    assert(result.delta == 0.0f);
    assert(result.field_18 == 0.0f);
    assert(result.field_1c == 50.0f);
    assert(result.patched_rect_height == 50.0f);

    return 0;
}
