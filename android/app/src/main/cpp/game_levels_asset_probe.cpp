#include "game_levels_asset_probe.h"

#include <filesystem>
#include <fstream>
#include <sstream>
#include <system_error>
#include <utility>
#include <vector>

#include "game_levels_scene_prefix.h"
#include "hp_data_reader.h"

namespace nevergone::game_levels_asset_probe {

Snapshot probe_file(const std::string& path, std::size_t max_bytes) {
    Snapshot result;
    result.configured = !path.empty();
    if (!result.configured) return result;

    std::error_code error;
    const std::filesystem::path file(path);
    result.present = std::filesystem::exists(file, error) && !error;
    if (!result.present) return result;

    error.clear();
    result.regular_file = std::filesystem::is_regular_file(file, error) && !error;
    if (!result.regular_file) return result;

    error.clear();
    const auto raw_size = std::filesystem::file_size(file, error);
    if (error) return result;
    result.file_size = static_cast<std::uint64_t>(raw_size);
    result.within_size_limit = raw_size <= max_bytes;
    if (!result.within_size_limit) return result;

    std::ifstream input(file, std::ios::binary);
    if (!input) return result;

    std::vector<std::uint8_t> bytes(static_cast<std::size_t>(raw_size));
    if (!bytes.empty()) {
        input.read(reinterpret_cast<char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
        if (!input || input.gcount() != static_cast<std::streamsize>(bytes.size())) return result;
    }

    hp_data::Reader reader(std::move(bytes));
    result.reader_size = reader.size();
    result.loaded = result.reader_size == static_cast<std::size_t>(raw_size);
    if (result.loaded) {
        game_levels_scene_prefix::Prefix prefix;
        result.scene_prefix_readable = game_levels_scene_prefix::parse(reader, &prefix);
        if (result.scene_prefix_readable) result.scene_prefix_bytes_consumed = prefix.bytes_consumed;

        game_levels_scene_prefix::FirstSceneHeader scene_header;
        result.first_scene_header_readable =
                game_levels_scene_prefix::parse_first_scene_header(reader, &scene_header);
        if (result.first_scene_header_readable) {
            result.first_scene_header_bytes_consumed = scene_header.bytes_consumed;
        }

        game_levels_scene_prefix::FirstLayerHeader layer_header;
        result.first_layer_header_readable =
                game_levels_scene_prefix::parse_first_layer_header(reader, &layer_header);
        if (result.first_layer_header_readable) {
            result.first_layer_header_bytes_consumed = layer_header.bytes_consumed;
        }

        game_levels_scene_prefix::FirstObjectPrefix object_prefix;
        result.first_object_prefix_readable =
                game_levels_scene_prefix::parse_first_object_prefix(reader, &object_prefix);
        if (result.first_object_prefix_readable) {
            result.first_object_prefix_bytes_consumed = object_prefix.bytes_consumed;
        }
    }
    return result;
}

Snapshot probe_pvp_scene(const std::string& files_dir) {
    if (files_dir.empty()) return {};
    const std::filesystem::path path =
            std::filesystem::path(files_dir) /
            "assets" /
            "gamescene" /
            "gs_list" /
            "pvp_scene.glData";
    return probe_file(path.string());
}

std::string status_report(const std::string& files_dir) {
    const Snapshot state = probe_pvp_scene(files_dir);
    std::ostringstream out;
    out << "GameLevels pvp scene: ";
    if (!state.configured) {
        out << "files-dir-unconfigured\n";
    } else if (!state.present) {
        out << "missing\n";
    } else if (!state.regular_file) {
        out << "not-regular-file\n";
    } else if (!state.within_size_limit) {
        out << "too-large (" << state.file_size << " bytes)\n";
    } else if (!state.loaded) {
        out << "read-failed\n";
    } else if (!state.scene_prefix_readable) {
        out << "loaded (" << state.reader_size << " bytes; LoadGL_Scene prefix truncated)\n";
    } else if (!state.first_scene_header_readable) {
        out << "loaded (" << state.reader_size << " bytes; prefix readable, first scene header unavailable)\n";
    } else if (!state.first_layer_header_readable) {
        out << "loaded (" << state.reader_size << " bytes; first scene header readable, first layer header unavailable)\n";
    } else if (!state.first_object_prefix_readable) {
        out << "loaded (" << state.reader_size << " bytes; first layer header readable, first object prefix unavailable)\n";
    } else {
        out << "loaded (" << state.reader_size << " bytes; first object prefix readable, "
            << state.first_object_prefix_bytes_consumed << " bytes verified)\n";
    }
    return out.str();
}

}  // namespace nevergone::game_levels_asset_probe
