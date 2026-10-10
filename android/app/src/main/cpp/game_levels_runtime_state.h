#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>

#include "game_levels_scene_instance.h"
#include "game_levels_scene_navigation.h"

namespace nevergone::game_levels_runtime_state {

constexpr std::size_t kMaxRuntimeSceneBytes = 64u * 1024u * 1024u;

enum class LoadStatus {
    kIdle = 0,
    kFilesDirUnconfigured,
    kMissing,
    kNotRegularFile,
    kTooLarge,
    kReadFailed,
    kParseFailed,
    kReady,
};

struct Snapshot {
    LoadStatus status = LoadStatus::kIdle;
    std::uint64_t load_attempt_count = 0;
    std::uint64_t file_size = 0;
    std::size_t reader_size = 0;
    std::size_t model_end_offset = 0;
    std::optional<std::size_t> current_port_node_index;
    std::optional<std::size_t> current_scene_index;
    std::string current_scene_guid;
    std::uint32_t stored_event_port_type = 0;
    bool current_scene_instance_ready = false;
    std::size_t current_scene_layer_count = 0;
    std::size_t current_scene_object_count = 0;
};

void reset();
bool load_file(const std::string& path, std::size_t max_bytes = kMaxRuntimeSceneBytes);
bool load_pvp_scene(const std::string& files_dir, std::size_t max_bytes = kMaxRuntimeSceneBytes);
Snapshot snapshot();
std::optional<game_levels_scene_instance::SceneInstance> current_scene_instance();
game_levels_scene_navigation::Transition step(std::uint32_t requested_event_port_type);
const char* status_name(LoadStatus status);
std::string status_report();

}  // namespace nevergone::game_levels_runtime_state
