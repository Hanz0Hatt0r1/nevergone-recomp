#include "game_scene_player_spawn_state.h"

#include <mutex>
#include <sstream>

namespace nevergone::game_scene_player_spawn_state {
namespace {

std::mutex g_mutex;
Snapshot g_state;
std::optional<game_scene_player_entry_markers::OfflinePosition> g_position;

void begin_refresh_locked(
        std::uint32_t event_port_type,
        int surface_width,
        int surface_height) {
    const std::uint64_t generation = g_state.generation + 1u;
    g_state = Snapshot{};
    g_state.generation = generation;
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

bool refresh(
        const game_levels_scene_instance::SceneInstance& scene,
        std::uint32_t event_port_type,
        int surface_width,
        int surface_height) {
    std::lock_guard<std::mutex> lock(g_mutex);
    begin_refresh_locked(event_port_type, surface_width, surface_height);

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
    if (markers->empty()) {
        g_state.status = Status::kNoMatchingMarker;
        return false;
    }

    // loadingTex() applies every matching type-6 marker in source traversal
    // order. MEPlayer::setPlayerPos() therefore leaves the last successful
    // transformed marker as the observable live position. Preserve the same
    // ordered overwrite semantics instead of inventing a uniqueness rule.
    for (const auto& marker : *markers) {
        const auto position = game_scene_player_entry_markers::transform_offline(
                marker,
                static_cast<float>(surface_width),
                static_cast<float>(surface_height));
        if (!position.has_value()) {
            g_state.status = Status::kTransformFailed;
            g_position.reset();
            return false;
        }

        ++g_state.applied_marker_count;
        g_state.source_layer_index = marker.source_layer_index;
        g_state.source_object_index = marker.source_object_index;
        g_state.source_x = marker.source_x;
        g_state.source_y = marker.source_y;
        g_state.player_x = position->x;
        g_state.player_y = position->y;
        g_position = *position;
    }

    g_state.status = Status::kReady;
    return true;
}

Snapshot snapshot() {
    std::lock_guard<std::mutex> lock(g_mutex);
    return g_state;
}

std::optional<game_scene_player_entry_markers::OfflinePosition> current_position() {
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
    out << "event port type: " << state.event_port_type << "\n";
    out << "surface: " << state.surface_width << "x" << state.surface_height << "\n";
    out << "matching markers: " << state.matching_marker_count << "\n";
    out << "applied markers: " << state.applied_marker_count << "\n";
    if (state.status == Status::kReady) {
        out << "source marker: layer " << state.source_layer_index
            << ", object " << state.source_object_index << "\n";
        out << "source point: " << state.source_x << ", " << state.source_y << "\n";
        out << "player point: " << state.player_x << ", " << state.player_y << "\n";
    }
    return out.str();
}

}  // namespace nevergone::game_scene_player_spawn_state
