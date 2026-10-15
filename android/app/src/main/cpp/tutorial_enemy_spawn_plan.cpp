#include "tutorial_enemy_spawn_plan.h"

#include <cmath>

namespace nevergone::tutorial_enemy_spawn_plan {
namespace {

constexpr float kSideRoom = 500.0f;
constexpr float kFirstOffset = 200.0f;
constexpr float kSecondOffset = 290.0f;
constexpr float kThirdOffset = 450.0f;
constexpr float kBlockOffset = 160.0f;
constexpr float kAirAttackOffset = 600.0f;
constexpr float kVerticalNearThreshold = 20.0f;
constexpr float kFallingRookYOffset = -120.0f;

int bounded_side(const Inputs& inputs) {
    if (inputs.player_is_face) {
        return inputs.player_x < kSideRoom ? 1 : -1;
    }
    const float right_limit = static_cast<float>(inputs.right_edge_x) - kSideRoom;
    return inputs.player_x <= right_limit ? 1 : -1;
}

struct VerticalOffsets {
    float second = 0.0f;
    float third = 0.0f;
};

VerticalOffsets wave_vertical_offsets(const Inputs& inputs) {
    const float edge_b_distance =
        std::fabs(static_cast<float>(inputs.vertical_edge_b) - inputs.player_y);
    if (edge_b_distance < kVerticalNearThreshold) {
        return {-30.0f, -10.0f};
    }

    const float edge_a_distance =
        std::fabs(static_cast<float>(inputs.vertical_edge_a) - inputs.player_y);
    if (edge_a_distance < kVerticalNearThreshold) {
        return {20.0f, 10.0f};
    }

    return {-20.0f, 20.0f};
}

EnemyRequest enemy(
        unsigned enemy_id,
        int enemy_state,
        int face_dir,
        float x,
        float y,
        bool delayed,
        float delay_seconds) {
    EnemyRequest request;
    request.enemy_id = enemy_id;
    request.enemy_state = enemy_state;
    request.player_face_dir = face_dir;
    request.x = x;
    request.y = y;
    request.delayed = delayed;
    request.delay_seconds = delay_seconds;
    return request;
}

Plan build_three_enemy_wave(Kind kind, const Inputs& inputs) {
    Plan plan;
    plan.kind = kind;
    plan.enemy_count = 3;

    const int side = bounded_side(inputs);
    const VerticalOffsets vertical = wave_vertical_offsets(inputs);
    const float side_f = static_cast<float>(side);
    const float second_delay = kind == Kind::kAttackWave ? 1.0f : 1.2f;

    plan.enemies[0] = enemy(
        3u,
        0x1c,
        inputs.player_face_dir,
        inputs.player_x + side_f * kFirstOffset,
        inputs.player_y,
        false,
        0.0f);
    plan.enemies[1] = enemy(
        3u,
        0x1c,
        inputs.player_face_dir,
        inputs.player_x + side_f * kSecondOffset,
        inputs.player_y + vertical.second,
        true,
        second_delay);
    plan.enemies[2] = enemy(
        3u,
        0x1c,
        inputs.player_face_dir,
        inputs.player_x + side_f * kThirdOffset,
        inputs.player_y + vertical.third,
        true,
        0.5f);

    if (kind == Kind::kAttackWave) {
        plan.effect.falling_rook = true;
        plan.effect.x = inputs.player_x + side_f * kSecondOffset;
        plan.effect.y = inputs.player_y + kFallingRookYOffset;
    }
    return plan;
}

}  // namespace

Plan build(Kind kind, const Inputs& inputs) {
    switch (kind) {
        case Kind::kAttackWave:
        case Kind::kExecutionWave:
            return build_three_enemy_wave(kind, inputs);

        case Kind::kBlockEnemy: {
            Plan plan;
            plan.kind = kind;
            plan.enemy_count = 1;
            const int side = bounded_side(inputs);
            plan.enemies[0] = enemy(
                3u,
                0x2f,
                inputs.player_face_dir,
                inputs.player_x + static_cast<float>(side) * kBlockOffset,
                inputs.player_y,
                false,
                0.0f);
            return plan;
        }

        case Kind::kAirAttackEnemy: {
            Plan plan;
            plan.kind = kind;
            plan.enemy_count = 1;
            const float side = inputs.player_is_face ? -1.0f : 1.0f;
            plan.enemies[0] = enemy(
                0x15u,
                0x14,
                inputs.player_face_dir,
                inputs.player_x + side * kAirAttackOffset,
                inputs.player_y,
                false,
                0.0f);
            return plan;
        }
    }

    return {};
}

}  // namespace nevergone::tutorial_enemy_spawn_plan
