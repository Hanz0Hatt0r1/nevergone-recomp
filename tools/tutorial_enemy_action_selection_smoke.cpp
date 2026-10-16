#include <cassert>
#include <cstring>

#include "tutorial_enemy_action_selection.h"

int main() {
    using nevergone::tutorial_enemy_action_selection::for_enemy;
    const auto* enemy = for_enemy(3);
    assert(enemy != nullptr);
    assert(std::strcmp(enemy->action_map, "enemy/enemy03/enemy_actions_1_03.plist") == 0);
    assert(enemy->start_action_id == 47);
    assert(std::strcmp(enemy->start_action_data,
                       "enemy/enemy03/adata/e03_e_born02b.actData") == 0);
    assert(!enemy->start_action_loops);
    assert(enemy->idle_action_id == 21);
    assert(std::strcmp(enemy->idle_action_data,
                       "enemy/enemy03/adata/e03_e_i01.actData") == 0);
    assert(enemy->idle_action_loops);
    assert(enemy->frames_per_second == 30);
    assert(for_enemy(4) == nullptr);
}
