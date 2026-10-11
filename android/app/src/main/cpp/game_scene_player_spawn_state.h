#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>

#include "game_levels_scene_instance.h"
#include "game_scene_player_bootstrap.h"
#include "game_scene_player_entry_markers.h"

namespace nevergone::game_scene_player_spawn_state {

enum class Status {
    kIdle = 0,
    kInvalidSurface,
    kMarkerCollectionFailed,
    kNoMatchingMarker,
    kTransformFailed,
    kReady,
};

struct Snapshot {
    Status status = Status::kIdle;
    std::uint64_t generation = 0;
    std::size_t source_scene_index = 0;
    std::string scene_guid;
    std::uint32_t event_port_type = 0;
    int surface_width = 0;
    int surface_height = 0;
    std::size_t matching_marker_count = 0;
    std::size_t applied_marker_count = 0;
    std::size_t source_layer_index = 0;
    std::size_t source_object_index = 0;
    float source_x = 0.0f;
    float source_y = 0.0f;
    float bootstrap_x = 0.0f;
    float bootstrap_y = 0.0f;
    float scene_offset_x = 0.0f;
    float scene_offset_y = 0.0f;
    float player_x = 0.0f;
    float player_y = 0.0f;
};

void reset();
bool refresh(
        const game_levels_scene_instance::SceneInstance& scene,
        std::uint32_t event_port_type,
        int surface_width,
        int surface_height);
Snapshot snapshot();
std::optional<game_scene_player_bootstrap::InitialPoint> current_position();
const char* status_name(Status status);
std::string status_report();

}  // namespace nevergone::game_scene_player_spawn_state
