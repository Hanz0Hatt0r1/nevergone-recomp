#include "game_levels_runtime_state.h"

#include <filesystem>
#include <fstream>
#include <mutex>
#include <optional>
#include <sstream>
#include <system_error>
#include <utility>
#include <vector>

#include "game_levels_model.h"
#include "game_levels_scene_instance.h"
#include "game_scene_construction_plan.h"
#include "hp_data_reader.h"

namespace nevergone::game_levels_runtime_state {
namespace {

std::mutex g_mutex;
std::optional<game_levels_model::Model> g_model;
std::optional<game_levels_scene_instance::SceneInstance> g_scene_instance;
std::optional<game_scene_construction_plan::ScenePlan> g_scene_construction_plan;
Snapshot g_state;

void refresh_current_scene_locked() {
    g_scene_instance.reset();
    g_scene_construction_plan.reset();
    g_state.current_port_node_index.reset();
    g_state.current_scene_index.reset();
    g_state.current_scene_guid.clear();
    g_state.stored_event_port_type = 0u;
    g_state.current_scene_instance_ready = false;
    g_state.current_scene_layer_count = 0u;
    g_state.current_scene_object_count = 0u;
    g_state.current_scene_construction_plan_ready = false;
    g_state.current_construction_layer_count = 0u;
    g_state.current_construction_object_count = 0u;
    g_state.current_construction_ignored_layer_count = 0u;
    if (!g_model.has_value()) return;

    const auto selection = game_levels_scene_navigation::resolve_current(
            g_model->scenes,
            g_model->port_nodes,
            g_model->navigation);
    g_state.current_port_node_index = selection.port_node_index;
    g_state.current_scene_index = selection.scene_index;
    g_state.current_scene_guid = selection.scene_guid;
    g_state.stored_event_port_type = g_model->navigation.stored_event_port_type;

    if (!selection.scene_index.has_value()) return;
    g_scene_instance = game_levels_scene_instance::build(
            *g_model,
            *selection.scene_index);
    if (!g_scene_instance.has_value()) return;

    g_state.current_scene_instance_ready = true;
    g_state.current_scene_layer_count = g_scene_instance->layers.size();
    g_state.current_scene_object_count = g_scene_instance->object_count;

    g_scene_construction_plan = game_scene_construction_plan::build(*g_scene_instance);
    g_state.current_scene_construction_plan_ready = true;
    g_state.current_construction_layer_count = g_scene_construction_plan->layers.size();
    g_state.current_construction_object_count = g_scene_construction_plan->object_count;
    g_state.current_construction_ignored_layer_count = g_scene_construction_plan->ignored_source_layer_count;
}

void begin_attempt_locked() {
    const std::uint64_t attempts = g_state.load_attempt_count + 1u;
    g_model.reset();
    g_scene_instance.reset();
    g_scene_construction_plan.reset();
    g_state = Snapshot{};
    g_state.load_attempt_count = attempts;
}

}  // namespace

void reset() {
    std::lock_guard<std::mutex> lock(g_mutex);
    g_model.reset();
    g_scene_instance.reset();
    g_scene_construction_plan.reset();
    g_state = Snapshot{};
}

bool load_file(const std::string& path, std::size_t max_bytes) {
    std::lock_guard<std::mutex> lock(g_mutex);
    begin_attempt_locked();
    if (path.empty()) {
        g_state.status = LoadStatus::kFilesDirUnconfigured;
        return false;
    }

    std::error_code error;
    const std::filesystem::path file(path);
    if (!std::filesystem::exists(file, error) || error) {
        g_state.status = LoadStatus::kMissing;
        return false;
    }
    error.clear();
    if (!std::filesystem::is_regular_file(file, error) || error) {
        g_state.status = LoadStatus::kNotRegularFile;
        return false;
    }
    error.clear();
    const auto raw_size = std::filesystem::file_size(file, error);
    if (error) {
        g_state.status = LoadStatus::kReadFailed;
        return false;
    }
    g_state.file_size = static_cast<std::uint64_t>(raw_size);
    if (raw_size > max_bytes) {
        g_state.status = LoadStatus::kTooLarge;
        return false;
    }

    std::ifstream input(file, std::ios::binary);
    if (!input) {
        g_state.status = LoadStatus::kReadFailed;
        return false;
    }
    std::vector<std::uint8_t> bytes(static_cast<std::size_t>(raw_size));
    if (!bytes.empty()) {
        input.read(reinterpret_cast<char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
        if (!input || input.gcount() != static_cast<std::streamsize>(bytes.size())) {
            g_state.status = LoadStatus::kReadFailed;
            return false;
        }
    }
    g_state.reader_size = bytes.size();

    hp_data::Reader reader(std::move(bytes));
    game_levels_model::Model parsed;
    if (!game_levels_model::parse(reader, &parsed)) {
        g_state.status = LoadStatus::kParseFailed;
        return false;
    }

    g_state.model_end_offset = parsed.end_offset;
    g_model = std::move(parsed);
    g_state.status = LoadStatus::kReady;
    refresh_current_scene_locked();
    return true;
}

bool load_pvp_scene(const std::string& files_dir, std::size_t max_bytes) {
    if (files_dir.empty()) return load_file({}, max_bytes);
    const std::filesystem::path path = std::filesystem::path(files_dir) /
            "assets" / "gamescene" / "gs_list" / "pvp_scene.glData";
    return load_file(path.string(), max_bytes);
}

Snapshot snapshot() {
    std::lock_guard<std::mutex> lock(g_mutex);
    return g_state;
}

std::optional<game_levels_scene_instance::SceneInstance> current_scene_instance() {
    std::lock_guard<std::mutex> lock(g_mutex);
    return g_scene_instance;
}

std::optional<game_scene_construction_plan::ScenePlan> current_scene_construction_plan() {
    std::lock_guard<std::mutex> lock(g_mutex);
    return g_scene_construction_plan;
}

game_levels_scene_navigation::Transition step(std::uint32_t requested_event_port_type) {
    std::lock_guard<std::mutex> lock(g_mutex);
    game_levels_scene_navigation::Transition result;
    if (!g_model.has_value()) return result;
    result = game_levels_scene_navigation::step_and_resolve(
            g_model->scenes,
            g_model->port_nodes,
            g_model->port_graph,
            requested_event_port_type,
            &g_model->navigation);
    refresh_current_scene_locked();
    return result;
}

const char* status_name(LoadStatus status) {
    switch (status) {
        case LoadStatus::kIdle: return "idle";
        case LoadStatus::kFilesDirUnconfigured: return "files-dir-unconfigured";
        case LoadStatus::kMissing: return "missing";
        case LoadStatus::kNotRegularFile: return "not-regular-file";
        case LoadStatus::kTooLarge: return "too-large";
        case LoadStatus::kReadFailed: return "read-failed";
        case LoadStatus::kParseFailed: return "parse-failed";
        case LoadStatus::kReady: return "ready";
    }
    return "unknown";
}

std::string status_report() {
    const Snapshot state = snapshot();
    std::ostringstream out;
    out << "GameLevels runtime model\n";
    out << "state: " << status_name(state.status) << "\n";
    out << "load attempts: " << state.load_attempt_count << "\n";
    if (state.reader_size != 0u) out << "scene bytes loaded: " << state.reader_size << "\n";
    if (state.model_end_offset != 0u) out << "verified model bytes: " << state.model_end_offset << "\n";
    if (state.current_port_node_index.has_value()) out << "current port index: " << *state.current_port_node_index << "\n";
    if (state.current_scene_index.has_value()) out << "current scene index: " << *state.current_scene_index << "\n";
    if (!state.current_scene_guid.empty()) out << "current scene guid: " << state.current_scene_guid << "\n";
    if (state.current_scene_instance_ready) {
        out << "scene instance: ready\n";
        out << "scene layers: " << state.current_scene_layer_count << "\n";
        out << "scene objects: " << state.current_scene_object_count << "\n";
    }
    if (state.current_scene_construction_plan_ready) {
        out << "scene construction plan: ready\n";
        out << "construction layers: " << state.current_construction_layer_count << "\n";
        out << "construction objects: " << state.current_construction_object_count << "\n";
        out << "ignored source layers: " << state.current_construction_ignored_layer_count << "\n";
    }
    return out.str();
}

}  // namespace nevergone::game_levels_runtime_state
