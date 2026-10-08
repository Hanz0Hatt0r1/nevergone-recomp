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
    bool first_record_header_readable = false;
    std::uint64_t file_size = 0;
    std::size_t reader_size = 0;
    std::size_t scene_prefix_bytes_consumed = 0;
    std::size_t first_record_header_bytes_consumed = 0;
};

// Probe an explicit file path. This is exposed for host regression tests and
// validates transport plus only the verified beginning of LoadGL_Scene. It
// never exposes proprietary parsed field values through diagnostics.
Snapshot probe_file(const std::string& path, std::size_t max_bytes = kMaxProbeBytes);

// Probe the recovered user-owned startup scene resource below
// <files>/assets/gamescene/gs_list/pvp_scene.glData.
Snapshot probe_pvp_scene(const std::string& files_dir);

std::string status_report(const std::string& files_dir);

}  // namespace nevergone::game_levels_asset_probe
