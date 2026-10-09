#include <cassert>
#include <iostream>

#include "single_select_hero_touch_state.h"

int main() {
    namespace touch = nevergone::single_select_hero_touch_state;

    touch::reset();
    auto state = touch::snapshot();
    assert(state.pointer_id == -1);
    assert(state.armed_tag == 0);
    assert(!state.inside);

    assert(!touch::begin(-1, 1));
    assert(!touch::begin(4, 0));
    assert(!touch::begin(4, 6));
    assert(touch::begin(4, 3));
    state = touch::snapshot();
    assert(state.pointer_id == 4);
    assert(state.armed_tag == 3);
    assert(state.inside);

    // A second pointer cannot steal the gesture captured by the first DOWN.
    assert(!touch::begin(7, 2));
    assert(!touch::move(7, false));

    // Leaving the original item's content box only changes pressed feedback;
    // the pointer remains captured and may re-enter before release.
    assert(touch::move(4, false));
    state = touch::snapshot();
    assert(state.pointer_id == 4);
    assert(state.armed_tag == 3);
    assert(!state.inside);
    assert(touch::move(4, true));
    assert(touch::snapshot().inside);

    int activated = -1;
    assert(touch::release(4, true, &activated));
    assert(activated == 3);
    state = touch::snapshot();
    assert(state.pointer_id == -1);
    assert(state.armed_tag == 0);

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
