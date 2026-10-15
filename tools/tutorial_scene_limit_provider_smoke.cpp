#include <cassert>
#include <optional>
#include <string>

#include "tutorial_scene_limit_provider.h"

int main() {
    namespace levels = nevergone::game_levels_runtime_state;
    namespace port = nevergone::game_levels_port_node_section;
    namespace provider = nevergone::tutorial_scene_limit_provider;

    levels::Snapshot runtime;
    runtime.status = levels::LoadStatus::kReady;
    runtime.load_attempt_count = 17;
    runtime.current_port_node_index = 2u;
    runtime.current_scene_guid = "cp-lesson-scene";

    port::PortNodeRecord record;
    record.first_string = "cp-lesson-scene";
    record.third_u32 = 111u;
    record.fourth_u32 = 222u;
    record.fifth_u32 = 333u;

    auto result = provider::resolve(runtime, record);
    assert(result.ready());
    assert(result.scene_limits.reviewed);
    assert(result.scene_limits.runtime_load_attempt_count == 17u);
    assert(result.scene_limits.scene_guid == "cp-lesson-scene");
    assert(result.scene_limits.vertical_edge_a == 111u);
    assert(result.scene_limits.vertical_edge_b == 222u);
    assert(result.scene_limits.right_edge_x == 333u);

    // Zero is a valid serialized unsigned value; the provider does not invent
    // a positivity rule absent from the recovered consumer.
    record.third_u32 = 0u;
    record.fourth_u32 = 0u;
    record.fifth_u32 = 0u;
    result = provider::resolve(runtime, record);
    assert(result.ready());
    assert(result.scene_limits.vertical_edge_a == 0u);
    assert(result.scene_limits.vertical_edge_b == 0u);
    assert(result.scene_limits.right_edge_x == 0u);

    // A record from another navigation generation/scene must fail closed.
    record.first_string = "other-scene";
    result = provider::resolve(runtime, record);
    assert(!result.ready());
    assert(result.port_node_available);
    assert(!result.port_scene_matches);
    assert(!result.scene_limits.reviewed);

    result = provider::resolve(runtime, std::nullopt);
    assert(!result.ready());
    assert(!result.port_node_available);

    runtime.status = levels::LoadStatus::kParseFailed;
    record.first_string = "cp-lesson-scene";
    result = provider::resolve(runtime, record);
    assert(!result.ready());
    assert(!result.runtime_ready);

    const std::string report = provider::status_report(result);
    assert(report.find("Tutorial scene-limit provider") != std::string::npos);
    assert(report.find("ready: no") != std::string::npos);

    return 0;
}
