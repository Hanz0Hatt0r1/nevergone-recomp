#pragma once

namespace nevergone::tutorial_enemy_action_selection {

struct Selection {
    unsigned enemy_id = 0;
    const char* action_map = nullptr;
    unsigned start_action_id = 0;
    const char* start_action_data = nullptr;
    const char* start_action_atlas = nullptr;
    bool start_action_loops = false;
    unsigned idle_action_id = 0;
    const char* idle_action_data = nullptr;
    const char* idle_action_atlas = nullptr;
    bool idle_action_loops = false;
    unsigned frames_per_second = 0;
};

// The original enemy03 action table sets start=47, maps action47 to the
// non-looping born02b clip, and maps looping action21 to its idle clip.
inline const Selection* for_enemy(unsigned enemy_id) {
    static constexpr Selection kTutorialEnemy3{
        3,
        "enemy/enemy03/enemy_actions_1_03.plist",
        47,
        "enemy/enemy03/adata/e03_e_born02b.actData",
        "e03_02_default.plist",
        false,
        21,
        "enemy/enemy03/adata/e03_e_i01.actData",
        "e03_default.plist",
        true,
        30,
    };
    return enemy_id == kTutorialEnemy3.enemy_id ? &kTutorialEnemy3 : nullptr;
}

}  // namespace nevergone::tutorial_enemy_action_selection
