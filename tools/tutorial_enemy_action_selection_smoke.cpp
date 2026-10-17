#include <cassert>
#include <cstring>

#include "tutorial_enemy_action_selection.h"

int main() {
    using nevergone::tutorial_enemy_action_selection::for_enemy;
    using nevergone::tutorial_enemy_action_selection::for_enemy_state;
    const auto* enemy = for_enemy(3);
    assert(enemy != nullptr);
    assert(std::strcmp(enemy->action_map, "enemy/enemy03/enemy_actions_1_03.plist") == 0);
    assert(enemy->start_action_id == 47);
    assert(std::strcmp(enemy->start_action_data,
                       "enemy/enemy03/adata/e03_e_born02b.actData") == 0);
    assert(!enemy->start_action_loops);
    assert(enemy->frames_per_second == 30);

    const auto* walking = for_enemy_state(3u, 1);
    assert(walking != nullptr && walking->action_id == 1u && walking->loops);
    assert(std::strcmp(walking->action_data,
                       "enemy/enemy03/adata/e03_e_w01.actData") == 0);
    const auto* running = for_enemy_state(3u, 2);
    assert(running != nullptr && running->action_id == 2u && running->loops);
    assert(std::strcmp(running->action_data,
                       "enemy/enemy03/adata/e03_e_r01.actData") == 0);
    const auto* attack1 = for_enemy_state(3u, 3);
    assert(attack1 != nullptr && attack1->action_id == 3u && !attack1->loops);
    assert(std::strcmp(attack1->action_data,
                       "enemy/enemy03/adata/e03_e_a01.actData") == 0);
    const auto* attack2 = for_enemy_state(3u, 4);
    assert(attack2 != nullptr && attack2->action_id == 4u && !attack2->loops);
    assert(std::strcmp(attack2->action_data,
                       "enemy/enemy03/adata/e03_e_a02.actData") == 0);
    const auto* idle = for_enemy_state(3u, 21);
    assert(idle != nullptr && idle->action_id == 21u && idle->loops);
    assert(std::strcmp(idle->action_data,
                       "enemy/enemy03/adata/e03_e_i01.actData") == 0);
    const auto* backward_walk = for_enemy_state(3u, 28);
    assert(backward_walk != nullptr && backward_walk->action_id == 28u &&
           backward_walk->loops);
    assert(std::strcmp(backward_walk->action_data,
                       "enemy/enemy03/adata/e03_e_w01_back.actData") == 0);
    assert(for_enemy_state(3u, 0x0f) == nullptr);
    assert(for_enemy_state(4u, 3) == nullptr);
    assert(for_enemy(4) == nullptr);
}
