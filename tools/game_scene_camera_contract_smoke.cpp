#include <cassert>
#include <cmath>

#include "game_scene_camera_contract.h"
#include "game_scene_camera_render_queue.h"

namespace camera = nevergone::game_scene_camera_contract;
namespace camera_queue = nevergone::game_scene_camera_render_queue;
namespace render_queue = nevergone::game_scene_render_queue;

namespace {

bool near(float lhs, float rhs) {
    return std::fabs(lhs - rhs) < 0.001f;
}

}  // namespace

int main() {
    // loadingTex(): x <= 0.62 * 1136 leaves the default camera eye at zero.
    auto eye = camera::initial_eye_x(704.0f, 2000.0f);
    assert(eye.has_value());
    assert(near(*eye, 0.0f));

    // 800 - (1136 * 0.62) = 95.68.
    eye = camera::initial_eye_x(800.0f, 2000.0f);
    assert(eye.has_value());
    assert(near(*eye, 95.68f));

    // Horizontal travel is capped at extent - half the design width.
    eye = camera::initial_eye_x(2000.0f, 900.0f);
    assert(eye.has_value());
    assert(near(*eye, 332.0f));

    // A too-small extent follows the native second clamp back to zero.
    eye = camera::initial_eye_x(1000.0f, 400.0f);
    assert(eye.has_value());
    assert(near(*eye, 0.0f));

    assert(!camera::initial_eye_x(NAN, 2000.0f).has_value());
    assert(!camera::initial_eye_x(800.0f, 2000.0f, 0.0f).has_value());

    // Generic class layer parent movement is (1-factor)*eye while the camera
    // shifts the complete world by -eye: net static shift is -factor*eye.
    auto shift = camera::static_screen_translation_x(0u, 1.0f, 100.0f);
    assert(shift.has_value() && near(*shift, -100.0f));
    shift = camera::static_screen_translation_x(3u, 0.5f, 100.0f);
    assert(shift.has_value() && near(*shift, -50.0f));
    shift = camera::static_screen_translation_x(5u, 0.0f, 100.0f);
    assert(shift.has_value() && near(*shift, 0.0f));

    // GameSceneClassLayer ID 8 == source layer index 7 pins parent X to zero.
    shift = camera::static_screen_translation_x(7u, 0.25f, 100.0f);
    assert(shift.has_value() && near(*shift, -100.0f));

    auto world_shift = camera::world_screen_translation_x(100.0f);
    assert(world_shift.has_value() && near(*world_shift, -100.0f));

    render_queue::Queue queue;
    render_queue::SpriteCommand far;
    far.source_layer_index = 2u;
    far.layer_camera_factor = 0.25f;
    far.transform.position_x = 400.0f;
    queue.sprites.push_back(far);
    render_queue::SpriteCommand fixed;
    fixed.source_layer_index = 7u;
    fixed.layer_camera_factor = 0.25f;
    fixed.transform.position_x = 400.0f;
    queue.sprites.push_back(fixed);

    queue = camera_queue::apply_static_world_camera(std::move(queue), 100.0f);
    assert(near(queue.sprites[0].transform.position_x, 375.0f));
    assert(near(queue.sprites[1].transform.position_x, 300.0f));
    return 0;
}
