#pragma once

#include <cstddef>
#include <vector>

namespace nevergone::battle_manager_player_intersection_contract {

struct Point {
    float x = 0.0f;
    float y = 0.0f;
};

struct Enemy {
    bool dead = false;
    int state = 0;
    bool byte_31f_nonzero = false;
    bool flying = false;
    // EnemyObject::enemyOrgHLPos() after enemyHLPos() has refreshed the
    // object's +0x184 point for its current state.
    Point origin_hit_point;
};

struct Result {
    bool intersects = false;
    bool side_flag_1e0 = false;
    bool side_flag_1e1 = false;
    std::size_t enemy_index = 0;
    // BattleManager +0x1d8; valid only when intersects is true.
    Point target_point;
};

// Mirrors BattleManager::playerIntersectsEnemy(CCPoint). The point passed to
// that function contributes only its X value; the candidate is the player's
// +0x130 point shifted by X. Enemy-array order is preserved and the first
// qualifying live enemy wins.
Result resolve(
        Point player_hit_point,
        float input_x_offset,
        int player_face_dir,
        const std::vector<Enemy>& enemies) noexcept;

}  // namespace nevergone::battle_manager_player_intersection_contract
