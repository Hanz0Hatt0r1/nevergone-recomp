#include "player_action_document_store.h"

#include <filesystem>
#include <fstream>
#include <mutex>
#include <sstream>
#include <system_error>
#include <utility>
#include <vector>

#include "hp_data_reader.h"

namespace nevergone::player_action_document_store {
namespace {

std::mutex g_mutex;
std::optional<enemy_actions_wbg_document::Document> g_document;
Snapshot g_state;

void begin_attempt_locked(const std::string& relative_path) {
    const std::uint64_t attempts = g_state.load_attempt_count + 1u;
    g_document.reset();
    g_state = Snapshot{};
    g_state.load_attempt_count = attempts;
    g_state.relative_path = relative_path;
}

bool safe_relative_path(const std::filesystem::path& path) {
    if (path.empty() || path.is_absolute()) return false;
    for (const auto& component : path) {
        if (component == "..") return false;
    }
    return true;
}

}  // namespace

void reset() {
    std::lock_guard<std::mutex> lock(g_mutex);
    g_document.reset();
    g_state = Snapshot{};
}

bool load_from_assets(
        const std::string& files_dir,
        const std::string& relative_path,
        std::size_t max_bytes) {
    std::lock_guard<std::mutex> lock(g_mutex);
    begin_attempt_locked(relative_path);

    if (files_dir.empty()) {
        g_state.status = LoadStatus::kFilesDirUnconfigured;
        return false;
    }

    const std::filesystem::path relative(relative_path);
    if (!safe_relative_path(relative)) {
        g_state.status = LoadStatus::kInvalidRelativePath;
        return false;
    }

    std::error_code error;
    const std::filesystem::path root = std::filesystem::canonical(
            std::filesystem::path(files_dir) / "assets", error);
    if (error || !std::filesystem::is_directory(root, error) || error) {
        g_state.status = LoadStatus::kAssetRootUnavailable;
        return false;
    }

    error.clear();
    const std::filesystem::path candidate = std::filesystem::weakly_canonical(root / relative, error);
    if (error) {
        g_state.status = LoadStatus::kMissing;
        return false;
    }

    const std::string root_prefix = root.string() + std::filesystem::path::preferred_separator;
    const std::string candidate_string = candidate.string();
    if (candidate_string != root.string() && candidate_string.rfind(root_prefix, 0u) != 0u) {
        g_state.status = LoadStatus::kPathEscapesAssetRoot;
        return false;
    }

    error.clear();
    if (!std::filesystem::exists(candidate, error) || error) {
        g_state.status = LoadStatus::kMissing;
        return false;
    }
    error.clear();
    if (!std::filesystem::is_regular_file(candidate, error) || error) {
        g_state.status = LoadStatus::kNotRegularFile;
        return false;
    }
    error.clear();
    const auto raw_size = std::filesystem::file_size(candidate, error);
    if (error) {
        g_state.status = LoadStatus::kReadFailed;
        return false;
    }
    g_state.file_size = static_cast<std::uint64_t>(raw_size);
    if (raw_size > max_bytes) {
        g_state.status = LoadStatus::kTooLarge;
        return false;
    }

    std::ifstream input(candidate, std::ios::binary);
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
    enemy_actions_wbg_document::Document parsed;
    if (!enemy_actions_wbg_document::parse(reader, &parsed)) {
        g_state.status = LoadStatus::kParseFailed;
        return false;
    }

    g_state.bytes_consumed = parsed.bytes_consumed;
    g_state.trailing_bytes = parsed.trailing_bytes;
    g_state.primary_record_count = parsed.primary_records.size();
    g_document = std::move(parsed);
    g_state.status = LoadStatus::kReady;
    return true;
}

Snapshot snapshot() {
    std::lock_guard<std::mutex> lock(g_mutex);
    return g_state;
}

std::optional<enemy_actions_wbg_document::Document> current_document() {
    std::lock_guard<std::mutex> lock(g_mutex);
    return g_document;
}

const char* status_name(LoadStatus status) {
    switch (status) {
        case LoadStatus::kIdle: return "idle";
        case LoadStatus::kFilesDirUnconfigured: return "files-dir-unconfigured";
        case LoadStatus::kInvalidRelativePath: return "invalid-relative-path";
        case LoadStatus::kAssetRootUnavailable: return "asset-root-unavailable";
        case LoadStatus::kPathEscapesAssetRoot: return "path-escapes-asset-root";
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
    out << "Player action document\n";
    out << "state: " << status_name(state.status) << "\n";
    out << "load attempts: " << state.load_attempt_count << "\n";
    if (!state.relative_path.empty()) out << "asset: " << state.relative_path << "\n";
    if (state.file_size != 0u) out << "file bytes: " << state.file_size << "\n";
    if (state.bytes_consumed != 0u) out << "parsed bytes: " << state.bytes_consumed << "\n";
    if (state.trailing_bytes != 0u) out << "trailing bytes: " << state.trailing_bytes << "\n";
    if (state.primary_record_count != 0u) {
        out << "primary records: " << state.primary_record_count << "\n";
    }
    return out.str();
}

}  // namespace nevergone::player_action_document_store
