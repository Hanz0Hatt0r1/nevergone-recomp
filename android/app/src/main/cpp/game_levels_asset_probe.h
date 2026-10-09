#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

namespace nevergone::game_levels_asset_probe {

constexpr std::size_t kMaxProbeBytes = 64u * 1024u * 1024u;

struct Snapshot {
    bool configured = false;
    bool present = false;
    bool regular_file = false;
    bool within_size_limit = false;
    bool loaded = false;
    bool scene_prefix_readable = false;
    bool first_scene_header_readable = false;
    bool first_layer_header_readable = false;
    bool first_object_prefix_readable = false;
    bool first_object_core_readable = false;
    bool first_object_version_extension_readable = false;
    bool first_object_conditional_header_readable = false;
    bool first_object_record_readable = false;
    bool first_layer_objects_readable = false;
    bool first_layer_record_readable = false;
    bool first_scene_layers_readable = false;
    std::uint64_t file_size = 0;
    std::size_t reader_size = 0;
    std::size_t scene_prefix_bytes_consumed = 0;
    std::size_t first_scene_header_bytes_consumed = 0;
    std::size_t first_layer_header_bytes_consumed = 0;
    std::size_t first_object_prefix_bytes_consumed = 0;
    std::size_t first_object_core_bytes_consumed = 0;
    std::size_t first_object_version_extension_bytes_consumed = 0;
    std::size_t first_object_conditional_header_bytes_consumed = 0;
    std::size_t first_object_record_bytes_consumed = 0;
    std::size_t first_layer_objects_bytes_consumed = 0;
    std::size_t first_layer_record_bytes_consumed = 0;
    std::size_t first_scene_layers_bytes_consumed = 0;
};

Snapshot probe_file(const std::string& path, std::size_t max_bytes = kMaxProbeBytes);
Snapshot probe_pvp_scene(const std::string& files_dir);
std::string status_report(const std::string& files_dir);

}  // namespace nevergone::game_levels_asset_probe
