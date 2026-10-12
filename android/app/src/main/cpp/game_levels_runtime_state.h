#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "game_levels_port_node_section.h"
#include "game_levels_scene_instance.h"
#include "game_levels_scene_navigation.h"
#include "game_scene_construction_plan.h"
#include "game_scene_render_queue.h"

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
    std::size_t atlas_preload_name_count = 0;
    std::optional<std::size_t> current_port_node_index;
    std::optional<std::size_t> current_scene_index;
    std::string current_scene_guid;
    std::uint32_t stored_event_port_type = 0;
    bool current_scene_instance_ready = false;
    std::size_t current_scene_layer_count = 0;
    std::size_t current_scene_object_count = 0;
    bool current_scene_construction_plan_ready = false;
    std::size_t current_construction_layer_count = 0;
    std::size_t current_construction_object_count = 0;
    std::size_t current_construction_ignored_layer_count = 0;
    std::size_t current_unresolved_type_distinct_count = 0;
    std::string current_unresolved_type_summary;
    bool current_scene_render_queue_ready = false;
    std::size_t current_render_sprite_count = 0;
    std::size_t current_render_sprite_frame_lookup_count = 0;
    std::size_t current_render_direct_file_count = 0;
    std::size_t current_render_scene_action_pair_count = 0;
    std::size_t current_render_unresolved_object_count = 0;
};

void reset();
bool load_file(const std::string& path, std::size_t max_bytes = kMaxRuntimeSceneBytes);
bool load_pvp_scene(const std::string& files_dir, std::size_t max_bytes = kMaxRuntimeSceneBytes);
Snapshot snapshot();
std::optional<game_levels_scene_instance::SceneInstance> current_scene_instance();
std::optional<game_scene_construction_plan::ScenePlan> current_scene_construction_plan();
std::optional<game_scene_render_queue::Queue> current_scene_render_queue();
// Exact parsed record selected by the current GameLevels navigation state.
// Camera reconstruction uses its retained fifth_u32 because loadingTex() copies
// GameScenePortNodeData +0x4b4 into GameScene +0x4b0 as the horizontal extent.
std::optional<game_levels_port_node_section::PortNodeRecord> current_port_node_record();
// Ordered strings retained from the leading LoadGL_Global resource array. The
// original first-playable loader iterates GameLevels + 0x24 and passes these
// names to CCSpriteFrameCache::addSpriteFramesWithFile().
std::vector<std::string> current_atlas_preload_names();
game_levels_scene_navigation::Transition step(std::uint32_t requested_event_port_type);
const char* status_name(LoadStatus status);
std::string status_report();

}  // namespace nevergone::game_levels_runtime_state
