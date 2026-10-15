#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace nevergone::tutorial_enemy_spawn_plan {

enum class Kind : int {
    kAttackWave = 1,
    kBlockEnemy = 2,
    kAirAttackEnemy = 3,
    kExecutionWave = 4,
};

struct Inputs {
    float player_x = 0.0f;
    float player_y = 0.0f;
    bool player_is_face = false;
    int player_face_dir = 0;

    // Raw unsigned scene limits consumed by the shipped LessonLayer from the
    // object reached through GameScene +0x238 -> +0x20.
    std::uint32_t vertical_edge_a = 0;
    std::uint32_t vertical_edge_b = 0;
    std::uint32_t right_edge_x = 0;
};

struct EnemyRequest {
    unsigned enemy_id = 0;
    int enemy_state = 0;
    int player_face_dir = 0;
    float x = 0.0f;
    float y = 0.0f;
    float delay_seconds = 0.0f;
    bool delayed = false;
};

struct EffectRequest {
    bool falling_rook = false;
    float x = 0.0f;
    float y = 0.0f;
};

struct Plan {
    Kind kind = Kind::kAttackWave;
    std::array<EnemyRequest, 3> enemies{};
    std::size_t enemy_count = 0;
    EffectRequest effect{};
};

Plan build(Kind kind, const Inputs& inputs);

}  // namespace nevergone::tutorial_enemy_spawn_plan
