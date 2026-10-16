#pragma once

namespace nevergone::tutorial_enemy_action_selection {

struct Selection {
    unsigned enemy_id = 0;
    const char* action_map = nullptr;
    unsigned start_action_id = 0;
    const char* start_action_data = nullptr;
    const char* start_action_atlas = nullptr;
    bool start_action_loops = false;
    unsigned state_action_id = 0;
    const char* state_action_data = nullptr;
    const char* state_action_atlas = nullptr;
    bool state_action_loops = false;
    unsigned frames_per_second = 0;
};

// The original enemy03 action table sets start=47 and contains looping action
// 28. The tutorial requests state 0x1c; treating that state as action index 28
// is the current candidate mapping, pending a direct runtime index trace.
inline const Selection* for_enemy(unsigned enemy_id) {
    static constexpr Selection kTutorialEnemy3{
        3,
        "enemy/enemy03/enemy_actions_1_03.plist",
        47,
        "enemy/enemy03/adata/e03_e_born02b.actData",
        "e03_02_default.plist",
        false,
        28,
        "enemy/enemy03/adata/e03_e_w01_back.actData",
        "e03_default.plist",
        true,
        30,
    };
    return enemy_id == kTutorialEnemy3.enemy_id ? &kTutorialEnemy3 : nullptr;
}

}  // namespace nevergone::tutorial_enemy_action_selection
