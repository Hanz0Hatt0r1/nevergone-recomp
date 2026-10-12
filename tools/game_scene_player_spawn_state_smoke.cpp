#include <cassert>
#include <cmath>
#include <limits>
#include <string>

#include "game_scene_player_spawn_state.h"

namespace {

nevergone::game_levels_scene_prefix::ObjectRecord marker(
        float x,
        float y,
        std::int32_t event_port_type) {
    nevergone::game_levels_scene_prefix::ObjectRecord object;
    object.first_i32 = 6;
    object.first_point_x = x;
    object.first_point_y = y;
    object.has_tail_i32 = true;
    object.tail_i32 = event_port_type;
    return object;
}

}  // namespace

int main() {
    namespace spawn = nevergone::game_scene_player_spawn_state;
    namespace scene_ns = nevergone::game_levels_scene_instance;

    scene_ns::SceneInstance scene;
    scene.source_scene_index = 3u;
    scene.guid = "scene-0";

    scene_ns::LayerInstance layer0;
    layer0.source_layer_index = 4u;
    layer0.objects.push_back(marker(600.0f, 300.0f, 7));
    // A different event-port marker must not affect this spawn.
    layer0.objects.push_back(marker(100.0f, 100.0f, 8));
    scene.layers.push_back(layer0);

    scene_ns::LayerInstance layer1;
    layer1.source_layer_index = 9u;
    layer1.objects.push_back(marker(650.0f, 350.0f, 7));
    scene.layers.push_back(layer1);

    spawn::reset();
    assert(spawn::refresh(scene, 7u, 1136, 640));
    auto state = spawn::snapshot();
    assert(state.status == spawn::Status::kReady);
    assert(state.generation == 1u);
    assert(state.source_scene_index == 3u);
    assert(state.scene_guid == "scene-0");
    assert(state.event_port_type == 7u);
    assert(state.matching_marker_count == 2u);
    assert(state.applied_marker_count == 2u);
    assert(state.used_entry_marker);
    // loadingTex traverses every match; the later matching marker wins.
    assert(state.source_layer_index == 9u);
    assert(state.source_object_index == 0u);
    assert(state.source_x == 650.0f);
    assert(state.source_y == 350.0f);
    assert(state.bootstrap_x == 568.0f);
    assert(state.bootstrap_y == 448.0f);
    assert(state.scene_offset_x == 82.0f);
    assert(state.scene_offset_y == 70.0f);
    assert(state.player_x == 650.0f);
    assert(state.player_y == 518.0f);
    const auto current = spawn::current_position();
    assert(current.has_value());
    assert(current->x == 650.0f);
    assert(current->y == 518.0f);
    assert(std::string(spawn::status_name(state.status)) == "ready");

    // Physical GLES size does not alter the Cocos design-space player point.
    assert(spawn::refresh(scene, 7u, 1920, 1080));
    state = spawn::snapshot();
    assert(state.generation == 2u);
    assert(state.surface_width == 1920);
    assert(state.surface_height == 1080);
    assert(state.player_x == 650.0f);
    assert(state.player_y == 518.0f);

    // A valid scene with no type-6 marker matching GameLevels +0x40 is not a
    // spawn failure in the original. The primary MEPlayer already exists and
    // loadingTex simply skips the conditional setPlayerPos() override.
    assert(spawn::refresh(scene, 99u, 1136, 640));
    state = spawn::snapshot();
    assert(state.status == spawn::Status::kReady);
    assert(state.source_scene_index == 3u);
    assert(state.scene_guid == "scene-0");
    assert(state.matching_marker_count == 0u);
    assert(state.applied_marker_count == 0u);
    assert(!state.used_entry_marker);
    assert(state.bootstrap_x == 568.0f);
    assert(state.bootstrap_y == 448.0f);
    assert(state.scene_offset_x == 0.0f);
    assert(state.scene_offset_y == 0.0f);
    assert(state.player_x == 568.0f);
    assert(state.player_y == 448.0f);
    const auto bootstrap_current = spawn::current_position();
    assert(bootstrap_current.has_value());
    assert(bootstrap_current->x == 568.0f);
    assert(bootstrap_current->y == 448.0f);

    assert(!spawn::refresh(scene, 7u, 0, 640));
    assert(spawn::snapshot().status == spawn::Status::kInvalidSurface);

    scene.layers[1].objects[0].first_point_x = std::numeric_limits<float>::quiet_NaN();
    assert(!spawn::refresh(scene, 7u, 1136, 640));
    assert(spawn::snapshot().status == spawn::Status::kMarkerCollectionFailed);
    assert(!spawn::current_position().has_value());

    scene.layers[1].objects[0] = marker(650.0f, 350.0f, 7);
    scene.layers[1].objects[0].has_tail_i32 = false;
    assert(!spawn::refresh(scene, 7u, 1136, 640));
    assert(spawn::snapshot().status == spawn::Status::kMarkerCollectionFailed);

    spawn::reset();
    assert(spawn::snapshot().status == spawn::Status::kIdle);
    assert(!spawn::current_position().has_value());
    return 0;
}
