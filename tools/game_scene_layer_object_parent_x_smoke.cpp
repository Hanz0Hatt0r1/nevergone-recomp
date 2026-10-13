#include <cassert>
#include <cmath>

#include "game_scene_layer_object_parent_x.h"

bool near(float a, float b) {
    return std::fabs(a - b) < 0.0001f;
}

int main() {
    namespace parent_x = nevergone::game_scene_layer_object_parent_x;

    // loadingTex z=0 creates class-layer ID 1: ordinary parallax formula.
    auto value = parent_x::from_move_input(0u, 0.25f, 100.0f);
    assert(value.has_value());
    assert(near(*value, 75.0f));

    // loadingTex z=7 creates class-layer ID 8: native branch forces X to zero.
    value = parent_x::from_move_input(7u, 0.25f, 100.0f);
    assert(value.has_value());
    assert(near(*value, 0.0f));

    // IDs 9..11 use a special Y branch, but X remains the ordinary formula.
    value = parent_x::from_move_input(8u, 1.2f, -50.0f);
    assert(value.has_value());
    assert(near(*value, 10.0f));
    value = parent_x::from_move_input(10u, 0.5f, 40.0f);
    assert(value.has_value());
    assert(near(*value, 20.0f));

    assert(!parent_x::from_move_input(11u, 1.0f, 10.0f).has_value());
    assert(!parent_x::from_move_input(0u, NAN, 10.0f).has_value());
    assert(!parent_x::from_move_input(0u, 1.0f, INFINITY).has_value());
    return 0;
}
