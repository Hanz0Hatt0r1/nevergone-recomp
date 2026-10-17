#include "battle_manager_player_intersection_contract.h"

#include <cmath>

namespace nevergone::battle_manager_player_intersection_contract {

Result resolve(
        Point player_hit_point,
        float input_x_offset,
        int player_face_dir,
        const std::vector<Enemy>& enemies) noexcept {
    Result result;
    const Point candidate{player_hit_point.x + input_x_offset, player_hit_point.y};
    if (!std::isfinite(candidate.x) || !std::isfinite(candidate.y)) return result;

    for (std::size_t index = 0; index < enemies.size(); ++index) {
        const Enemy& enemy = enemies[index];
        if (enemy.dead || enemy.state == 0x27 || enemy.state == 0x24 ||
                enemy.byte_31f_nonzero || enemy.flying ||
                !std::isfinite(enemy.origin_hit_point.x) ||
                !std::isfinite(enemy.origin_hit_point.y)) {
            continue;
        }

        const float dx = candidate.x - enemy.origin_hit_point.x;
        const float dy = candidate.y - enemy.origin_hit_point.y;
        if (!(std::hypot(dx, dy) < 80.0f) || !(std::fabs(dy) <= 30.0f)) continue;

        result.intersects = true;
        result.enemy_index = index;
        result.target_point = enemy.origin_hit_point;

        if (player_face_dir == 0) {
            result.side_flag_1e1 = true;
            result.target_point.x += candidate.x > enemy.origin_hit_point.x ? 80.0f : -80.0f;
        } else if (player_face_dir == 1) {
            result.side_flag_1e0 = true;
            result.target_point.x += candidate.x < enemy.origin_hit_point.x ? -80.0f : 80.0f;
        }
        return result;
    }

    return result;
}

}  // namespace nevergone::battle_manager_player_intersection_contract
