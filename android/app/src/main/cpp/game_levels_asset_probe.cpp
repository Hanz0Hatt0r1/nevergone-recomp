#include "game_levels_asset_probe.h"

#include <filesystem>
#include <fstream>
#include <sstream>
#include <system_error>
#include <vector>

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
    } else {
        out << "loaded (" << state.reader_size << " bytes)\n";
    }
    return out.str();
}

}  // namespace nevergone::game_levels_asset_probe
