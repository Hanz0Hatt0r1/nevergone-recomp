#include <cassert>
#include <cmath>

#include "scene_actions_primary_frame_plan.h"

namespace {
bool near(float a, float b) { return std::fabs(a - b) < 0.0001f; }
}

int main() {
    namespace frame_plan = nevergone::scene_actions_primary_frame_plan;
    namespace base = nevergone::scene_actions_base_sprite_update;

    nevergone::trap_actions_document::Document doc;
    doc.core.primary.records.resize(2u);
    auto& frame = doc.core.primary.records[1];
    frame.first_string = "effect.plist";
    frame.third_string = "effect_02.png";
    frame.float_values[0] = 20.0f;
    frame.float_values[1] = 40.0f;
    frame.float_values[2] = 0.25f;
    frame.float_values[3] = 0.75f;
    frame.float_values[6] = 80.0f;
    frame.float_values[7] = 100.0f;
    frame.float_values[9] = 30.0f;
    frame.float_values[10] = 1.5f;
    frame.float_values[11] = 0.75f;

    base::SystemInputs system;
    system.scale_240 = 2.0f;
    system.position_offset_x_244 = 100.0f;
    system.position_offset_y_248 = 50.0f;

    const auto normal = frame_plan::build(doc, 1u, system);
    assert(normal.has_value());
    assert(normal->frame_index == 1u);
    assert(normal->atlas_plist == "effect.plist");
    assert(normal->frame_name == "effect_02.png");
    assert(near(normal->position_x, 60.0f));
    assert(near(normal->position_y, 45.0f));
    assert(near(normal->anchor_x, 0.25f));
    assert(near(normal->rotation, 30.0f));
    assert(!normal->flip_x);

    system.flip_x_15e = true;
    const auto reflected = frame_plan::build(doc, 1u, system);
    assert(reflected.has_value());
    assert(reflected->atlas_plist == "effect.plist");
    assert(reflected->frame_name == "effect_02.png");
    assert(near(reflected->position_x, 20.0f));
    assert(near(reflected->position_y, 45.0f));
    assert(near(reflected->anchor_x, 0.75f));
    assert(near(reflected->rotation, -30.0f));
    assert(reflected->flip_x);

    assert(!frame_plan::build(doc, 2u, system).has_value());
    auto missing = doc;
    missing.core.primary.records[1].third_string.clear();
    assert(!frame_plan::build(missing, 1u, system).has_value());
    return 0;
}
