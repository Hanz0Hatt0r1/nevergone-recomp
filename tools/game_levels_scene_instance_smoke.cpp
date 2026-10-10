#include <cassert>
#include <cstddef>
#include <string>

#include "game_levels_model.h"
#include "game_levels_scene_instance.h"

int main() {
    namespace instance = nevergone::game_levels_scene_instance;
    namespace layers = nevergone::game_levels_layer_tail;
    namespace objects = nevergone::game_levels_scene_prefix;

    nevergone::game_levels_model::Model model;

    layers::SceneRecord first;
    first.string_value = "scene-a";
    first.first_point_x = 10.0f;
    first.first_point_y = 20.0f;

    layers::LayerRecord layer0;
    layer0.first_float = 0.5f;
    objects::ObjectRecord object0;
    object0.first_i32 = 7;
    object0.string_value = "object-a";
    object0.first_point_x = 1.0f;
    object0.first_point_y = 2.0f;
    layer0.objects.push_back(object0);
    layer0.top_border_points.push_back({3.0f, 4.0f});
    first.layers.push_back(layer0);

    layers::LayerRecord layer1;
    layer1.first_float = 1.5f;
    objects::ObjectRecord object1;
    object1.first_i32 = 8;
    object1.string_value = "object-b";
    layer1.objects.push_back(object1);
    objects::ObjectRecord object2;
    object2.first_i32 = 9;
    object2.string_value = "object-c";
    layer1.objects.push_back(object2);
    layer1.bottom_border_points.push_back({5.0f, 6.0f});
    first.layers.push_back(layer1);

    layers::SceneRecord second;
    second.string_value = "scene-b";
    model.scenes.scenes = {first, second};

    nevergone::game_levels_port_node_section::PortNodeRecord port;
    port.first_string = "scene-a";
    model.port_nodes.port_nodes.push_back(port);
    model.navigation.current_port_node_index = 0u;

    const auto built = instance::build(model, 0u);
    assert(built.has_value());
    assert(built->source_scene_index == 0u);
    assert(built->guid == "scene-a");
    assert(built->first_point_x == 10.0f);
    assert(built->first_point_y == 20.0f);
    assert(built->layers.size() == 2u);
    assert(built->object_count == 3u);
    assert(built->layers[0].source_layer_index == 0u);
    assert(built->layers[0].first_float == 0.5f);
    assert(built->layers[0].objects.size() == 1u);
    assert(built->layers[0].objects[0].string_value == "object-a");
    assert(built->layers[0].top_border_points.size() == 1u);
    assert(built->layers[1].source_layer_index == 1u);
    assert(built->layers[1].objects.size() == 2u);
    assert(built->layers[1].bottom_border_points.size() == 1u);

    const auto current = instance::build_current(model);
    assert(current.has_value());
    assert(current->source_scene_index == 0u);
    assert(current->guid == "scene-a");

    // The scene instance owns a structural copy, so later model mutations do
    // not invalidate renderer-facing data.
    model.scenes.scenes[0].string_value = "mutated";
    model.scenes.scenes[0].layers[0].objects[0].string_value = "mutated-object";
    assert(built->guid == "scene-a");
    assert(built->layers[0].objects[0].string_value == "object-a");

    assert(!instance::build(model, 99u).has_value());
    model.navigation.current_port_node_index.reset();
    assert(!instance::build_current(model).has_value());
    return 0;
}
