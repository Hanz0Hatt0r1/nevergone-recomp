#include <cassert>
#include <cmath>
#include <cstdint>

#include "game_scene_player_entry_markers.h"

using nevergone::game_levels_scene_instance::LayerInstance;
using nevergone::game_levels_scene_instance::SceneInstance;
using nevergone::game_levels_scene_prefix::ObjectRecord;
using nevergone::game_scene_player_entry_markers::collect;
using nevergone::game_scene_player_entry_markers::transform_offline;

namespace {

ObjectRecord object(
        std::int32_t type,
        std::int32_t tail,
        float x,
        float y,
        bool has_tail = true) {
    ObjectRecord value;
    value.first_i32 = type;
    value.has_tail_i32 = has_tail;
    value.tail_i32 = tail;
    value.first_point_x = x;
    value.first_point_y = y;
    return value;
}

}  // namespace

int main() {
    SceneInstance scene;

    LayerInstance layer0;
    layer0.source_layer_index = 3;
    layer0.objects.push_back(object(0, 0, 1.0f, 2.0f, false));
    layer0.objects.push_back(object(6, 1, 10.0f, 20.0f));
    layer0.objects.push_back(object(6, 0, 600.0f, 300.0f));
    scene.layers.push_back(layer0);

    LayerInstance layer1;
    layer1.source_layer_index = 7;
    layer1.objects.push_back(object(6, 0, -5.5f, 8.25f));
    scene.layers.push_back(layer1);

    // loadingTex only visits ordered layer slots 0..10. A matching marker in
    // source layer slot 11 must not enter the project-owned selection result.
    for (std::size_t i = 2; i < 11; ++i) {
        LayerInstance filler;
        filler.source_layer_index = i + 10;
        scene.layers.push_back(filler);
    }
    LayerInstance ignored;
    ignored.source_layer_index = 99;
    ignored.objects.push_back(object(6, 0, 99.0f, 99.0f));
    scene.layers.push_back(ignored);

    auto event0 = collect(scene, 0u);
    assert(event0.has_value());
    assert(event0->size() == 2u);
    assert((*event0)[0].source_layer_index == 3u);
    assert((*event0)[0].source_object_index == 2u);
    assert((*event0)[0].event_port_type == 0u);
    assert((*event0)[0].source_x == 600.0f);
    assert((*event0)[0].source_y == 300.0f);
    assert((*event0)[1].source_layer_index == 7u);
    assert((*event0)[1].source_object_index == 0u);
    assert((*event0)[1].source_x == -5.5f);
    assert((*event0)[1].source_y == 8.25f);

    auto position = transform_offline((*event0)[0], 1136.0f, 640.0f);
    assert(position.has_value());
    assert(position->x == 32.0f);
    assert(position->y == 20.0f);

    auto event1 = collect(scene, 1u);
    assert(event1.has_value());
    assert(event1->size() == 1u);
    assert((*event1)[0].source_object_index == 1u);
    assert((*event1)[0].source_x == 10.0f);
    assert((*event1)[0].source_y == 20.0f);

    SceneInstance malformed;
    LayerInstance bad_layer;
    bad_layer.objects.push_back(object(6, 0, 1.0f, 2.0f, false));
    malformed.layers.push_back(bad_layer);
    assert(!collect(malformed, 0u).has_value());

    SceneInstance non_finite;
    LayerInstance nan_layer;
    nan_layer.objects.push_back(object(6, 0, std::nanf(""), 2.0f));
    non_finite.layers.push_back(nan_layer);
    assert(!collect(non_finite, 0u).has_value());

    const auto marker = (*event0)[0];
    assert(!transform_offline(marker, 0.0f, 640.0f).has_value());
    assert(!transform_offline(marker, 1136.0f, -1.0f).has_value());
    assert(!transform_offline(marker, std::nanf(""), 640.0f).has_value());

    return 0;
}
