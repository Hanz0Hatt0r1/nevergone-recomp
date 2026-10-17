#include "game_scene_player_spawn_state.h"

#include <mutex>
#include <sstream>
#include <utility>

#include "game_scene_direct_geometry.h"

namespace nevergone::game_scene_player_spawn_state {
namespace {

std::mutex g_mutex;
Snapshot g_state;
std::optional<game_scene_player_bootstrap::InitialPoint> g_position;

void begin_refresh_locked(
        const game_levels_scene_instance::SceneInstance& scene,
        std::uint32_t event_port_type,
        int surface_width,
        int surface_height) {
    const std::uint64_t generation = g_state.generation + 1u;
    g_state = Snapshot{};
    g_state.generation = generation;
    g_state.source_scene_index = scene.source_scene_index;
    g_state.scene_guid = scene.guid;
    g_state.player_name = game_scene_player_bootstrap::player_name();
    g_state.event_port_type = event_port_type;
    g_state.surface_width = surface_width;
    g_state.surface_height = surface_height;
    g_position.reset();
}

}  // namespace

void reset() {
    std::lock_guard<std::mutex> lock(g_mutex);
    g_state = Snapshot{};
    g_position.reset();
}

void set_player_name(std::string player_name) {
    std::lock_guard<std::mutex> lock(g_mutex);
    if (g_state.status == Status::kReady) {
        g_state.player_name = std::move(player_name);
    }
}

bool refresh(
        const game_levels_scene_instance::SceneInstance& scene,
        std::uint32_t event_port_type,
        int surface_width,
        int surface_height) {
    std::lock_guard<std::mutex> lock(g_mutex);
    begin_refresh_locked(scene, event_port_type, surface_width, surface_height);

    // Surface dimensions still gate live renderer readiness and identify the
    // active GLES revision. Player coordinates themselves are Cocos design-space
    // values because the original configured 1136x640 ExactFit before querying
    // CCDirector::getWinSize().
    if (surface_width <= 0 || surface_height <= 0) {
        g_state.status = Status::kInvalidSurface;
        return false;
    }

    const auto markers = game_scene_player_entry_markers::collect(scene, event_port_type);
    if (!markers.has_value()) {
        g_state.status = Status::kMarkerCollectionFailed;
        return false;
    }
    g_state.matching_marker_count = markers->size();

    const auto bootstrap = game_scene_player_bootstrap::initial_manager_point(
            game_scene_direct_geometry::kDesignWidth,
            game_scene_direct_geometry::kDesignHeight);
    if (!bootstrap.has_value()) {
        g_state.status = Status::kTransformFailed;
        return false;
    }
    g_state.bootstrap_x = bootstrap->x;
    g_state.bootstrap_y = bootstrap->y;

    if (markers->empty()) {
        // The original MEPlayer already exists at the manager construction
        // point before GameScene::loadingTex() walks type-6 objects. The
        // setPlayerPos() override is conditional on a type-6 tail matching
        // GameLevels +0x40; with no match, loadingTex() simply keeps the
        // bootstrap position and continues scene construction.
        g_state.used_entry_marker = false;
        g_state.applied_marker_count = 0u;
        g_state.scene_offset_x = 0.0f;
        g_state.scene_offset_y = 0.0f;
        g_state.player_x = bootstrap->x;
        g_state.player_y = bootstrap->y;
        g_position = *bootstrap;
        g_state.status = Status::kReady;
        return true;
    }

    const auto resolved = game_scene_player_entry_markers::resolve_offline_player_position(
            scene,
            event_port_type,
            game_scene_direct_geometry::kDesignWidth,
            game_scene_direct_geometry::kDesignHeight);
    if (!resolved.has_value()) {
        g_state.status = Status::kTransformFailed;
        return false;
    }

    // loadingTex() executes setPlayerPos() for every matching marker. The
    // resolved helper models the resulting final state (the last match) and the
    // two-part MEPlayer position: bootstrap + scene offset.
    g_state.used_entry_marker = true;
    g_state.applied_marker_count = markers->size();
    g_state.source_layer_index = resolved->marker.source_layer_index;
    g_state.source_object_index = resolved->marker.source_object_index;
    g_state.source_x = resolved->marker.source_x;
    g_state.source_y = resolved->marker.source_y;
    g_state.bootstrap_x = resolved->bootstrap_position.x;
    g_state.bootstrap_y = resolved->bootstrap_position.y;
    g_state.scene_offset_x = resolved->scene_offset.x;
    g_state.scene_offset_y = resolved->scene_offset.y;
    g_state.player_x = resolved->visible_position.x;
    g_state.player_y = resolved->visible_position.y;
    g_position = resolved->visible_position;

    g_state.status = Status::kReady;
    return true;
}

Snapshot snapshot() {
    std::lock_guard<std::mutex> lock(g_mutex);
    return g_state;
}

std::optional<game_scene_player_bootstrap::InitialPoint> current_position() {
    std::lock_guard<std::mutex> lock(g_mutex);
    return g_position;
}

const char* status_name(Status status) {
    switch (status) {
        case Status::kIdle: return "idle";
        case Status::kInvalidSurface: return "invalid-surface";
        case Status::kMarkerCollectionFailed: return "marker-collection-failed";
        case Status::kNoMatchingMarker: return "no-matching-marker";
        case Status::kTransformFailed: return "transform-failed";
        case Status::kReady: return "ready";
    }
    return "unknown";
}

std::string status_report() {
    const Snapshot state = snapshot();
    std::ostringstream out;
    out << "GameScene player spawn state\n";
    out << "state: " << status_name(state.status) << "\n";
    out << "generation: " << state.generation << "\n";
    if (!state.player_name.empty()) {
        out << "player name: " << state.player_name << "\n";
    }
    if (!state.scene_guid.empty()) {
        out << "scene: " << state.source_scene_index << " (" << state.scene_guid << ")\n";
    }
    out << "event port type: " << state.event_port_type << "\n";
    out << "surface: " << state.surface_width << "x" << state.surface_height << "\n";
    out << "matching markers: " << state.matching_marker_count << "\n";
    out << "applied markers: " << state.applied_marker_count << "\n";
    if (state.status == Status::kReady) {
        out << "entry marker applied: " << (state.used_entry_marker ? "yes" : "no") << "\n";
        if (state.used_entry_marker) {
            out << "source marker: layer " << state.source_layer_index
                << ", object " << state.source_object_index << "\n";
            out << "source point: " << state.source_x << ", " << state.source_y << "\n";
        }
        out << "bootstrap point: " << state.bootstrap_x << ", " << state.bootstrap_y << "\n";
        out << "scene offset: " << state.scene_offset_x << ", " << state.scene_offset_y << "\n";
        out << "player point: " << state.player_x << ", " << state.player_y << "\n";
    }
    return out.str();
}

}  // namespace nevergone::game_scene_player_spawn_state
