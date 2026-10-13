#include <cassert>
#include <cstdint>

#include "game_scene_object_event_id.h"

int main() {
    using nevergone::game_levels_scene_prefix::ObjectRecord;
    namespace event_id = nevergone::game_scene_object_event_id;

    ObjectRecord absent;
    absent.conditional_present = false;
    absent.conditional_first_u32 = 2001u;
    assert(event_id::from_record(absent) == 0);

    ObjectRecord first;
    first.conditional_present = true;
    first.conditional_first_u32 = 2001u;
    assert(event_id::from_record(first) == 2001);

    ObjectRecord last;
    last.conditional_present = true;
    last.conditional_first_u32 = 2019u;
    assert(event_id::from_record(last) == 2019);

    ObjectRecord bits;
    bits.conditional_present = true;
    bits.conditional_first_u32 = 0xffffffffu;
    assert(event_id::from_record(bits) == -1);

    return 0;
}
