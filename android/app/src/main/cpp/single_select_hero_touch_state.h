#pragma once

namespace nevergone::single_select_hero_touch_state {

struct Snapshot {
    int pointer_id = -1;
    int armed_tag = 0;
    bool inside = false;
};

void reset();
Snapshot snapshot();
bool begin(int pointer_id, int tag);
bool move(int pointer_id, bool inside);
bool release(int pointer_id, bool inside, int* activated_tag);
bool cancel(int pointer_id);

}  // namespace nevergone::single_select_hero_touch_state
