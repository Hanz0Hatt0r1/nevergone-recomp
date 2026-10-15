#include <cassert>
#include <cmath>

#include "../android/app/src/main/cpp/tutorial_enemy_spawn_plan.h"

using nevergone::tutorial_enemy_spawn_plan::Inputs;
using nevergone::tutorial_enemy_spawn_plan::Kind;
using nevergone::tutorial_enemy_spawn_plan::build;

namespace {

bool near(float left, float right) {
    return std::fabs(left - right) < 0.0001f;
}

void assert_enemy(
        const nevergone::tutorial_enemy_spawn_plan::EnemyRequest& enemy,
        unsigned id,
        int state,
        int face_dir,
        float x,
        float y,
        bool delayed,
        float delay) {
    assert(enemy.enemy_id == id);
    assert(enemy.enemy_state == state);
    assert(enemy.player_face_dir == face_dir);
    assert(near(enemy.x, x));
    assert(near(enemy.y, y));
    assert(enemy.delayed == delayed);
    assert(near(enemy.delay_seconds, delay));
}

}  // namespace

int main() {
    Inputs input;
    input.player_x = 600.0f;
    input.player_y = 100.0f;
    input.player_is_face = false;
    input.player_face_dir = 7;
    input.vertical_edge_a = 0;
    input.vertical_edge_b = 110;
    input.right_edge_x = 2000;

    auto plan = build(Kind::kAttackWave, input);
    assert(plan.enemy_count == 3);
    assert_enemy(plan.enemies[0], 3, 0x1c, 7, 800.0f, 100.0f, false, 0.0f);
    assert_enemy(plan.enemies[1], 3, 0x1c, 7, 890.0f, 70.0f, true, 1.0f);
    assert_enemy(plan.enemies[2], 3, 0x1c, 7, 1050.0f, 90.0f, true, 0.5f);
    assert(plan.effect.falling_rook);
    assert(near(plan.effect.x, 890.0f));
    assert(near(plan.effect.y, -20.0f));

    // Near the opposite vertical edge, the shipped branches move the delayed
    // enemies above the player's Y instead of below it.
    input.vertical_edge_b = 500;
    input.vertical_edge_a = 105;
    plan = build(Kind::kAttackWave, input);
    assert_enemy(plan.enemies[1], 3, 0x1c, 7, 890.0f, 120.0f, true, 1.0f);
    assert_enemy(plan.enemies[2], 3, 0x1c, 7, 1050.0f, 110.0f, true, 0.5f);

    // Away from both vertical edges the two delayed enemies straddle Y.
    input.vertical_edge_a = 0;
    input.vertical_edge_b = 500;
    plan = build(Kind::kAttackWave, input);
    assert_enemy(plan.enemies[1], 3, 0x1c, 7, 890.0f, 80.0f, true, 1.0f);
    assert_enemy(plan.enemies[2], 3, 0x1c, 7, 1050.0f, 120.0f, true, 0.5f);

    // player_is_face=false normally uses the positive-X side but falls back to
    // negative X when less than 500 design units remain at the right edge.
    input.player_x = 1600.0f;
    input.right_edge_x = 2000;
    plan = build(Kind::kAttackWave, input);
    assert_enemy(plan.enemies[0], 3, 0x1c, 7, 1400.0f, 100.0f, false, 0.0f);
    assert_enemy(plan.enemies[1], 3, 0x1c, 7, 1310.0f, 80.0f, true, 1.0f);
    assert_enemy(plan.enemies[2], 3, 0x1c, 7, 1150.0f, 120.0f, true, 0.5f);

    // player_is_face=true selects negative X once x >= 500, but uses the
    // opposite side if the player is too close to the left edge.
    input.player_is_face = true;
    input.player_x = 500.0f;
    plan = build(Kind::kAttackWave, input);
    assert(near(plan.enemies[0].x, 300.0f));
    input.player_x = 499.0f;
    plan = build(Kind::kAttackWave, input);
    assert(near(plan.enemies[0].x, 699.0f));

    input.player_x = 600.0f;
    input.player_is_face = false;
    plan = build(Kind::kExecutionWave, input);
    assert(plan.enemy_count == 3);
    assert(!plan.effect.falling_rook);
    assert_enemy(plan.enemies[0], 3, 0x1c, 7, 800.0f, 100.0f, false, 0.0f);
    assert_enemy(plan.enemies[1], 3, 0x1c, 7, 890.0f, 80.0f, true, 1.2f);
    assert_enemy(plan.enemies[2], 3, 0x1c, 7, 1050.0f, 120.0f, true, 0.5f);

    plan = build(Kind::kBlockEnemy, input);
    assert(plan.enemy_count == 1);
    assert_enemy(plan.enemies[0], 3, 0x2f, 7, 760.0f, 100.0f, false, 0.0f);

    input.player_is_face = true;
    input.player_x = 700.0f;
    plan = build(Kind::kBlockEnemy, input);
    assert_enemy(plan.enemies[0], 3, 0x2f, 7, 540.0f, 100.0f, false, 0.0f);

    input.player_is_face = false;
    input.player_x = 600.0f;
    plan = build(Kind::kAirAttackEnemy, input);
    assert(plan.enemy_count == 1);
    assert_enemy(plan.enemies[0], 0x15, 0x14, 7, 1200.0f, 100.0f, false, 0.0f);
    input.player_is_face = true;
    plan = build(Kind::kAirAttackEnemy, input);
    assert_enemy(plan.enemies[0], 0x15, 0x14, 7, 0.0f, 100.0f, false, 0.0f);

    return 0;
}
