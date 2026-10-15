#include <cassert>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include "game_levels_runtime_state.h"

namespace {
void append_u32(std::vector<std::uint8_t>* out, std::uint32_t value) {
    for (unsigned shift = 0; shift < 32; shift += 8) out->push_back(static_cast<std::uint8_t>(value >> shift));
}
void append_i32(std::vector<std::uint8_t>* out, std::int32_t value) { append_u32(out, static_cast<std::uint32_t>(value)); }
void append_f32(std::vector<std::uint8_t>* out, float value) {
    std::uint32_t bits = 0; std::memcpy(&bits, &value, sizeof(bits)); append_u32(out, bits);
}
void append_string(std::vector<std::uint8_t>* out, const std::string& value, std::uint8_t gap) {
    append_u32(out, static_cast<std::uint32_t>(value.size()));
    out->push_back(gap);
    out->insert(out->end(), value.begin(), value.end());
}
void append_port(std::vector<std::uint8_t>* out, const std::string& first, bool start, const std::string& third) {
    append_string(out, first, 0x31u);
    append_string(out, "", 0x32u);
    append_u32(out, 1u); append_u32(out, 2u);
    out->push_back(start ? 1u : 0u); out->push_back(0u); out->push_back(0u);
    append_u32(out, 3u); append_u32(out, 4u); append_u32(out, 5u);
    append_string(out, third, 0x33u);
}
void append_enemy_record(
        std::vector<std::uint8_t>* out,
        std::int32_t first,
        std::int32_t second,
        float first_float,
        std::int32_t third,
        std::int32_t fourth,
        std::int32_t fifth,
        std::int32_t sixth,
        std::int32_t seventh) {
    append_i32(out, first);
    append_i32(out, second);
    append_f32(out, first_float);
    append_i32(out, third);
    append_i32(out, fourth);
    append_i32(out, fifth);
    append_i32(out, sixth);
    append_i32(out, seventh);
}
std::vector<std::uint8_t> fixture() {
    std::vector<std::uint8_t> out;
    append_i32(&out, 2); append_u32(&out, 2u);
    append_string(&out, "a", 0x11u); append_f32(&out, 1.0f); append_f32(&out, 2.0f); append_u32(&out, 1u);
    append_f32(&out, 0.5f); append_u32(&out, 0u); append_u32(&out, 0u); append_u32(&out, 0u);
    append_string(&out, "b", 0x12u); append_f32(&out, 3.0f); append_f32(&out, 4.0f); append_u32(&out, 0u);
    append_u32(&out, 0u);
    append_u32(&out, 2u);
    append_string(&out, "pvp_scene_default.plist", 0x41u);
    append_string(&out, "pvp_scene_effect.plist", 0x42u);
    append_u32(&out, 11u); append_u32(&out, 12u); append_u32(&out, 0u); append_u32(&out, 0u);
    append_u32(&out, 2u);
    append_enemy_record(&out, -101, 202, 3.25f, -303, 404, -505, 606, -707);
    append_enemy_record(&out, 808, -909, -10.5f, 1111, -1212, 1313, -1414, 1515);
    append_u32(&out, 2u);
    append_port(&out, "a", true, "b");
    append_port(&out, "b", false, "");
    return out;
}
}  // namespace

int main() {
    namespace runtime = nevergone::game_levels_runtime_state;
    namespace port_nav = nevergone::game_levels_port_navigation;

    const auto root = std::filesystem::temp_directory_path() / "nevergone-runtime-state-smoke";
    std::filesystem::remove_all(root);
    const auto path = root / "assets" / "gamescene" / "gs_list" / "pvp_scene.glData";
    std::filesystem::create_directories(path.parent_path());
    const auto bytes = fixture();
    {
        std::ofstream output(path, std::ios::binary);
        output.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
        assert(output.good());
    }

    runtime::reset();
    assert(runtime::snapshot().status == runtime::LoadStatus::kIdle);
    assert(runtime::current_atlas_preload_names().empty());
    assert(runtime::current_global_enemy_records().empty());
    assert(!runtime::current_scene_instance().has_value());
    assert(!runtime::current_scene_construction_plan().has_value());
    assert(!runtime::current_scene_render_queue().has_value());
    assert(runtime::load_pvp_scene(root.string()));
    auto state = runtime::snapshot();
    assert(state.status == runtime::LoadStatus::kReady);
    assert(state.load_attempt_count == 1u);
    assert(state.reader_size == bytes.size());
    assert(state.model_end_offset == bytes.size());
    assert(state.atlas_preload_name_count == 2u);
    assert(state.global_enemy_record_count == 2u);
    const auto atlas_names = runtime::current_atlas_preload_names();
    assert(atlas_names.size() == 2u);
    assert(atlas_names[0] == "pvp_scene_default.plist");
    assert(atlas_names[1] == "pvp_scene_effect.plist");
    const auto enemy_records = runtime::current_global_enemy_records();
    assert(enemy_records.size() == 2u);
    assert(enemy_records[0].first_i32 == -101);
    assert(enemy_records[0].second_i32 == 202);
    assert(enemy_records[0].first_float == 3.25f);
    assert(enemy_records[0].third_i32 == -303);
    assert(enemy_records[0].fourth_i32 == 404);
    assert(enemy_records[0].fifth_i32 == -505);
    assert(enemy_records[0].sixth_i32 == 606);
    assert(enemy_records[0].seventh_i32 == -707);
    assert(enemy_records[1].first_i32 == 808);
    assert(enemy_records[1].second_i32 == -909);
    assert(enemy_records[1].first_float == -10.5f);
    assert(enemy_records[1].third_i32 == 1111);
    assert(enemy_records[1].fourth_i32 == -1212);
    assert(enemy_records[1].fifth_i32 == 1313);
    assert(enemy_records[1].sixth_i32 == -1414);
    assert(enemy_records[1].seventh_i32 == 1515);
    const std::string report = runtime::status_report();
    assert(report.find("global enemy records: 2\n") != std::string::npos);
    assert(report.find("global enemy[0]: -101,202,3.25,-303,404,-505,606,-707\n") != std::string::npos);
    assert(report.find("global enemy[1]: 808,-909,-10.5,1111,-1212,1313,-1414,1515\n") != std::string::npos);
    assert(state.current_port_node_index == 0u);
    assert(state.current_scene_index == 0u);
    assert(state.current_scene_guid == "a");
    assert(state.stored_event_port_type == 0u);
    assert(state.current_scene_instance_ready);
    assert(state.current_scene_layer_count == 1u);
    assert(state.current_scene_object_count == 0u);
    assert(state.current_scene_construction_plan_ready);
    assert(state.current_construction_layer_count == 1u);
    assert(state.current_construction_object_count == 0u);
    assert(state.current_construction_ignored_layer_count == 0u);
    assert(state.current_scene_render_queue_ready);
    assert(state.current_render_sprite_count == 0u);
    assert(state.current_render_sprite_frame_lookup_count == 0u);
    assert(state.current_render_direct_file_count == 0u);
    assert(state.current_render_scene_action_pair_count == 0u);
    assert(state.current_render_unresolved_object_count == 0u);

    auto scene = runtime::current_scene_instance();
    assert(scene.has_value());
    assert(scene->source_scene_index == 0u);
    assert(scene->guid == "a");
    assert(scene->first_point_x == 1.0f);
    assert(scene->first_point_y == 2.0f);
    assert(scene->layers.size() == 1u);
    assert(scene->layers[0].first_float == 0.5f);

    auto plan = runtime::current_scene_construction_plan();
    assert(plan.has_value());
    assert(plan->source_scene_index == scene->source_scene_index);
    assert(plan->guid == "a");
    assert(plan->layers.size() == 1u);
    assert(plan->layers[0].z_index == 0u);
    assert(plan->layers[0].source_layer_index == scene->layers[0].source_layer_index);
    assert(plan->object_count == 0u);
    assert(plan->ignored_source_layer_count == 0u);

    auto queue = runtime::current_scene_render_queue();
    assert(queue.has_value());
    assert(queue->source_scene_index == 0u);
    assert(queue->guid == "a");
    assert(queue->sprites.empty());

    auto transition = runtime::step(1u);
    assert(transition.port_step.status == port_nav::StepStatus::kTraversed);
    state = runtime::snapshot();
    assert(state.current_port_node_index == 1u);
    assert(state.current_scene_index == 1u);
    assert(state.current_scene_guid == "b");
    assert(state.stored_event_port_type == 0u);
    assert(state.current_scene_instance_ready);
    assert(state.current_scene_layer_count == 0u);
    assert(state.current_scene_construction_plan_ready);
    assert(state.current_construction_layer_count == 0u);
    assert(state.current_scene_render_queue_ready);
    assert(state.atlas_preload_name_count == 2u);
    assert(state.global_enemy_record_count == 2u);
    assert(runtime::current_atlas_preload_names() == atlas_names);
    assert(runtime::current_global_enemy_records().size() == 2u);
    scene = runtime::current_scene_instance();
    assert(scene.has_value());
    assert(scene->source_scene_index == 1u);
    assert(scene->guid == "b");
    plan = runtime::current_scene_construction_plan();
    assert(plan.has_value());
    assert(plan->source_scene_index == 1u);
    assert(plan->guid == "b");
    assert(plan->layers.empty());
    queue = runtime::current_scene_render_queue();
    assert(queue.has_value());
    assert(queue->source_scene_index == 1u);
    assert(queue->guid == "b");
    assert(queue->sprites.empty());

    transition = runtime::step(0u);
    assert(transition.port_step.status == port_nav::StepStatus::kTraversed);
    state = runtime::snapshot();
    assert(state.current_port_node_index == 0u);
    assert(state.current_scene_index == 0u);
    assert(state.current_scene_guid == "a");
    assert(state.stored_event_port_type == 1u);
    assert(state.current_scene_instance_ready);
    assert(state.current_scene_construction_plan_ready);
    assert(state.current_scene_render_queue_ready);
    assert(runtime::current_scene_instance()->layers.size() == 1u);
    assert(runtime::current_scene_construction_plan()->layers.size() == 1u);
    assert(runtime::current_scene_render_queue()->guid == "a");

    transition = runtime::step(9u);
    assert(transition.port_step.status == port_nav::StepStatus::kUnsupportedEventType);
    assert(runtime::snapshot().current_scene_guid == "a");
    assert(runtime::current_scene_instance()->guid == "a");
    assert(runtime::current_scene_construction_plan()->guid == "a");
    assert(runtime::current_scene_render_queue()->guid == "a");

    assert(!runtime::load_file(path.string(), 1u));
    assert(runtime::snapshot().status == runtime::LoadStatus::kTooLarge);
    assert(runtime::snapshot().load_attempt_count == 2u);
    assert(runtime::snapshot().atlas_preload_name_count == 0u);
    assert(runtime::snapshot().global_enemy_record_count == 0u);
    assert(runtime::current_atlas_preload_names().empty());
    assert(runtime::current_global_enemy_records().empty());
    assert(!runtime::current_scene_instance().has_value());
    assert(!runtime::current_scene_construction_plan().has_value());
    assert(!runtime::current_scene_render_queue().has_value());
    assert(!runtime::snapshot().current_scene_render_queue_ready);

    assert(!runtime::load_pvp_scene((root / "missing-root").string()));
    assert(runtime::snapshot().status == runtime::LoadStatus::kMissing);
    assert(runtime::snapshot().load_attempt_count == 3u);
    assert(runtime::snapshot().global_enemy_record_count == 0u);
    assert(runtime::current_atlas_preload_names().empty());
    assert(runtime::current_global_enemy_records().empty());
    assert(!runtime::current_scene_instance().has_value());
    assert(!runtime::current_scene_construction_plan().has_value());
    assert(!runtime::current_scene_render_queue().has_value());

    std::filesystem::remove_all(root);
    runtime::reset();
    assert(runtime::snapshot().status == runtime::LoadStatus::kIdle);
    assert(runtime::snapshot().global_enemy_record_count == 0u);
    assert(runtime::current_atlas_preload_names().empty());
    assert(runtime::current_global_enemy_records().empty());
    assert(!runtime::current_scene_instance().has_value());
    assert(!runtime::current_scene_construction_plan().has_value());
    assert(!runtime::current_scene_render_queue().has_value());
    return 0;
}
