#include <cassert>
#include <iostream>

#include "single_select_hero_touch_state.h"

int main() {
    namespace touch = nevergone::single_select_hero_touch_state;
    touch::reset();
    assert(touch::snapshot().pointer_id == -1);

    assert(!touch::begin(-1, 1));
    assert(!touch::begin(1, 0));
    assert(!touch::begin(1, 6));
    assert(touch::begin(4, 3));
    assert(!touch::begin(7, 2));

    auto state = touch::snapshot();
    assert(state.pointer_id == 4);
    assert(state.armed_tag == 3);
    assert(state.inside);

    assert(touch::move(4, false));
    assert(!touch::snapshot().inside);
    assert(touch::move(4, true));
    assert(touch::snapshot().inside);

    int activated = -1;
    assert(touch::release(4, true, &activated));
    assert(activated == 3);
    assert(touch::snapshot().pointer_id == -1);

    assert(touch::begin(2, 5));
    activated = -1;
    assert(touch::release(2, false, &activated));
    assert(activated == 0);

    assert(touch::begin(9, 1));
    assert(!touch::cancel(8));
    assert(touch::cancel(9));
    assert(touch::snapshot().pointer_id == -1);

    std::cout << "single-select-hero touch state smoke: ok\n";
    return 0;
}
