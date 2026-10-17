#pragma once

namespace nevergone::tutorial_enemy_action_selection {

struct StateAction {
    unsigned action_id = 0;
    const char* action_data = nullptr;
    const char* atlas = nullptr;
    bool loops = false;
};

struct Selection {
    unsigned enemy_id = 0;
    const char* action_map = nullptr;
    unsigned start_action_id = 0;
    const char* start_action_data = nullptr;
    const char* start_action_atlas = nullptr;
    bool start_action_loops = false;
    unsigned frames_per_second = 0;
};

// onlineEnemyState passes the requested state directly to CCArray::objectAtIndex
// on EnemyObject+0x104. These reviewed entries come from enemy_actions_1_03.plist.
inline const StateAction* for_enemy_state(unsigned enemy_id, int state_id) {
    static constexpr StateAction kEnemy3Walking{
        1, "enemy/enemy03/adata/e03_e_w01.actData", "e03_default.plist", true};
    static constexpr StateAction kEnemy3Running{
        2, "enemy/enemy03/adata/e03_e_r01.actData", "e03_default.plist", true};
    static constexpr StateAction kEnemy3Attack1{
        3, "enemy/enemy03/adata/e03_e_a01.actData", "e03_default.plist", false};
    static constexpr StateAction kEnemy3Attack2{
        4, "enemy/enemy03/adata/e03_e_a02.actData", "e03_default.plist", false};
    static constexpr StateAction kEnemy3Idle{
        21, "enemy/enemy03/adata/e03_e_i01.actData", "e03_default.plist", true};
    static constexpr StateAction kEnemy3WalkingBack{
        28, "enemy/enemy03/adata/e03_e_w01_back.actData", "e03_default.plist", true};

    if (enemy_id != 3u) return nullptr;
    switch (state_id) {
        case 1: return &kEnemy3Walking;
        case 2: return &kEnemy3Running;
        case 3: return &kEnemy3Attack1;
        case 4: return &kEnemy3Attack2;
        case 21: return &kEnemy3Idle;
        case 28: return &kEnemy3WalkingBack;
        default: return nullptr;
    }
}

// The original enemy03 table sets start=47 and the tutorial requests state
// 0x1c. Native lookup confirms that 0x1c is the action-array index 28.
inline const Selection* for_enemy(unsigned enemy_id) {
    static constexpr Selection kTutorialEnemy3{
        3,
        "enemy/enemy03/enemy_actions_1_03.plist",
        47,
        "enemy/enemy03/adata/e03_e_born02b.actData",
        "e03_02_default.plist",
        false,
        30,
    };
    return enemy_id == kTutorialEnemy3.enemy_id ? &kTutorialEnemy3 : nullptr;
}

}  // namespace nevergone::tutorial_enemy_action_selection
