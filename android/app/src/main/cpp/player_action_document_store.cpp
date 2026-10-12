#include "player_action_document_store.h"

#include <filesystem>
#include <fstream>
#include <limits>
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

void begin_attempt_locked(
        const std::string& relative_path,
        const std::string& embedded_entry_name = {}) {
    const std::uint64_t attempts = g_state.load_attempt_count + 1u;
    g_document.reset();
    g_state = Snapshot{};
    g_state.load_attempt_count = attempts;
    g_state.relative_path = relative_path;
    g_state.embedded_entry_name = embedded_entry_name;
}

bool safe_relative_path(const std::filesystem::path& path) {
    if (path.empty() || path.is_absolute()) return false;
    for (const auto& component : path) {
        if (component == "..") return false;
    }
    return true;
}

bool safe_bundle_entry_name(const std::string& value) {
    if (value.empty() || value.size() > kMaxBundleEntryNameBytes) return false;
    for (const unsigned char byte : value) {
        if (byte == 0u || byte == '\n' || byte == '\r' || byte == '/' || byte == '\\') return false;
    }
    return value != "." && value != "..";
}

bool resolve_asset_file_locked(
        const std::string& files_dir,
        const std::string& relative_path,
        std::size_t max_bytes,
        std::filesystem::path* candidate_out,
        std::uint64_t* raw_size_out) {
    if (candidate_out == nullptr || raw_size_out == nullptr) return false;
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

    *candidate_out = candidate;
    *raw_size_out = static_cast<std::uint64_t>(raw_size);
    return true;
}

bool parse_document_locked(std::vector<std::uint8_t> bytes) {
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

bool read_line_bounded(
        std::ifstream* input,
        std::uint64_t file_size,
        std::uint64_t* cursor,
        std::size_t max_bytes,
        std::string* output) {
    if (input == nullptr || cursor == nullptr || output == nullptr) return false;
    output->clear();
    while (*cursor < file_size) {
        char value = 0;
        input->get(value);
        if (!*input) return false;
        ++*cursor;
        if (value == '\n') return true;
        if (value == '\r' || value == '\0' || output->size() >= max_bytes) return false;
        output->push_back(value);
    }
    return false;
}

bool parse_decimal_size(const std::string& text, std::uint64_t* output) {
    if (output == nullptr || text.empty()) return false;
    std::uint64_t value = 0;
    for (const char ch : text) {
        if (ch < '0' || ch > '9') return false;
        const std::uint64_t digit = static_cast<std::uint64_t>(ch - '0');
        if (value > (std::numeric_limits<std::uint64_t>::max() - digit) / 10u) return false;
        value = value * 10u + digit;
    }
    *output = value;
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

    std::filesystem::path candidate;
    std::uint64_t raw_size = 0;
    if (!resolve_asset_file_locked(files_dir, relative_path, max_bytes, &candidate, &raw_size)) {
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
    return parse_document_locked(std::move(bytes));
}

bool load_embedded_from_assets(
        const std::string& files_dir,
        const std::string& bundle_relative_path,
        const std::string& entry_name,
        std::size_t max_bundle_bytes,
        std::size_t max_entry_bytes) {
    std::lock_guard<std::mutex> lock(g_mutex);
    begin_attempt_locked(bundle_relative_path, entry_name);

    if (!safe_bundle_entry_name(entry_name)) {
        g_state.status = LoadStatus::kBundleInvalidEntryName;
        return false;
    }

    std::filesystem::path candidate;
    std::uint64_t raw_size = 0;
    if (!resolve_asset_file_locked(
            files_dir, bundle_relative_path, max_bundle_bytes, &candidate, &raw_size)) {
        return false;
    }

    std::ifstream input(candidate, std::ios::binary);
    if (!input) {
        g_state.status = LoadStatus::kReadFailed;
        return false;
    }

    std::uint64_t cursor = 0;
    std::size_t entry_count = 0;
    std::size_t match_count = 0;
    std::vector<std::uint8_t> selected;
    while (cursor < raw_size) {
        if (entry_count >= kMaxBundleEntries) {
            g_state.status = LoadStatus::kBundleMalformed;
            return false;
        }
        ++entry_count;

        std::string name;
        if (!read_line_bounded(
                &input, raw_size, &cursor, kMaxBundleEntryNameBytes, &name) ||
            !safe_bundle_entry_name(name)) {
            g_state.status = LoadStatus::kBundleMalformed;
            return false;
        }

        std::string size_text;
        if (!read_line_bounded(&input, raw_size, &cursor, 20u, &size_text)) {
            g_state.status = LoadStatus::kBundleMalformed;
            return false;
        }
        std::uint64_t payload_size = 0;
        if (!parse_decimal_size(size_text, &payload_size) || payload_size > raw_size - cursor) {
            g_state.status = LoadStatus::kBundleMalformed;
            return false;
        }

        if (name == entry_name) {
            ++match_count;
            if (match_count > 1u) {
                g_state.status = LoadStatus::kBundleEntryAmbiguous;
                return false;
            }
            if (payload_size > max_entry_bytes ||
                payload_size > static_cast<std::uint64_t>(std::numeric_limits<std::size_t>::max())) {
                g_state.status = LoadStatus::kTooLarge;
                return false;
            }
            selected.resize(static_cast<std::size_t>(payload_size));
            if (!selected.empty()) {
                input.read(
                        reinterpret_cast<char*>(selected.data()),
                        static_cast<std::streamsize>(selected.size()));
                if (!input || input.gcount() != static_cast<std::streamsize>(selected.size())) {
                    g_state.status = LoadStatus::kReadFailed;
                    return false;
                }
            }
        } else {
            input.seekg(static_cast<std::streamoff>(payload_size), std::ios::cur);
            if (!input) {
                g_state.status = LoadStatus::kReadFailed;
                return false;
            }
        }
        cursor += payload_size;

        if (cursor < raw_size) {
            char delimiter = 0;
            input.get(delimiter);
            if (!input) {
                g_state.status = LoadStatus::kReadFailed;
                return false;
            }
            ++cursor;
            if (delimiter != '\n') {
                g_state.status = LoadStatus::kBundleMalformed;
                return false;
            }
        }
    }

    if (match_count == 0u) {
        g_state.status = LoadStatus::kBundleEntryMissing;
        return false;
    }
    return parse_document_locked(std::move(selected));
}

Snapshot snapshot() {
    std::lock_guard<std::mutex> lock(g_mutex);
    return g_state;
}

std::optional<enemy_actions_wbg_document::Document> current_document() {
    std::lock_guard<std::mutex> lock(g_mutex);
    return g_document;
}

std::optional<enemy_actions_wbg_prefix::ActionFrameRecord> initial_frame_record() {
    std::lock_guard<std::mutex> lock(g_mutex);
    if (!g_document.has_value() || g_document->primary_records.empty()) return std::nullopt;
    return g_document->primary_records.front();
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
        case LoadStatus::kBundleInvalidEntryName: return "bundle-invalid-entry-name";
        case LoadStatus::kBundleMalformed: return "bundle-malformed";
        case LoadStatus::kBundleEntryMissing: return "bundle-entry-missing";
        case LoadStatus::kBundleEntryAmbiguous: return "bundle-entry-ambiguous";
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
    if (!state.embedded_entry_name.empty()) {
        out << "embedded entry: " << state.embedded_entry_name << "\n";
    }
    if (state.file_size != 0u) out << "file bytes: " << state.file_size << "\n";
    if (state.reader_size != 0u) out << "reader bytes: " << state.reader_size << "\n";
    if (state.bytes_consumed != 0u) out << "parsed bytes: " << state.bytes_consumed << "\n";
    if (state.trailing_bytes != 0u) out << "trailing bytes: " << state.trailing_bytes << "\n";
    if (state.primary_record_count != 0u) {
        out << "primary records: " << state.primary_record_count << "\n";
    }
    return out.str();
}

}  // namespace nevergone::player_action_document_store
