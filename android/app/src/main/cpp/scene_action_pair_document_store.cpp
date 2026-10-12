#include "scene_action_pair_document_store.h"

#include <filesystem>
#include <fstream>
#include <mutex>
#include <sstream>
#include <system_error>
#include <utility>
#include <vector>

#include "hp_data_reader.h"

namespace nevergone::scene_action_pair_document_store {
namespace {

struct SlotState {
    SlotSnapshot snapshot;
    std::optional<trap_actions_document::Document> document;
};

std::mutex g_mutex;
int g_variant = 0;
SlotState g_born;
SlotState g_idle;

SlotState& slot_state(Slot slot) {
    return slot == Slot::kBorn ? g_born : g_idle;
}

const SlotState& slot_state_const(Slot slot) {
    return slot == Slot::kBorn ? g_born : g_idle;
}

bool safe_relative_path(const std::filesystem::path& path) {
    if (path.empty() || path.is_absolute()) return false;
    for (const auto& component : path) {
        if (component == "..") return false;
    }
    return true;
}

void begin_attempt(SlotState* state, const std::string& relative_path) {
    const std::uint64_t attempts = state->snapshot.load_attempt_count + 1u;
    state->document.reset();
    state->snapshot = SlotSnapshot{};
    state->snapshot.load_attempt_count = attempts;
    state->snapshot.relative_path = relative_path;
}

bool parse_file(
        SlotState* state,
        const std::filesystem::path& candidate,
        std::uint64_t raw_size) {
    std::ifstream input(candidate, std::ios::binary);
    if (!input) {
        state->snapshot.status = LoadStatus::kReadFailed;
        return false;
    }
    std::vector<std::uint8_t> bytes(static_cast<std::size_t>(raw_size));
    if (!bytes.empty()) {
        input.read(reinterpret_cast<char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
        if (!input || input.gcount() != static_cast<std::streamsize>(bytes.size())) {
            state->snapshot.status = LoadStatus::kReadFailed;
            return false;
        }
    }

    hp_data::Reader reader(std::move(bytes));
    trap_actions_document::Document parsed;
    if (!trap_actions_document::parse(reader, &parsed)) {
        state->snapshot.status = LoadStatus::kParseFailed;
        return false;
    }
    state->snapshot.bytes_consumed = parsed.bytes_consumed;
    state->snapshot.trailing_bytes = parsed.trailing_bytes;
    state->snapshot.primary_record_count = parsed.core.primary.records.size();
    state->document = std::move(parsed);
    state->snapshot.status = LoadStatus::kReady;
    return true;
}

}  // namespace

void reset() {
    std::lock_guard<std::mutex> lock(g_mutex);
    g_variant = 0;
    g_born = SlotState{};
    g_idle = SlotState{};
}

bool load_from_assets(
        Slot slot,
        int variant,
        const std::string& files_dir,
        const std::string& relative_path,
        std::size_t max_bytes) {
    std::lock_guard<std::mutex> lock(g_mutex);
    SlotState& state = slot_state(slot);
    begin_attempt(&state, relative_path);

    if (variant < 1 || variant > 4) {
        state.snapshot.status = LoadStatus::kInvalidRelativePath;
        return false;
    }
    if (g_variant != 0 && g_variant != variant) {
        g_born.document.reset();
        g_idle.document.reset();
        g_born.snapshot = SlotSnapshot{};
        g_idle.snapshot = SlotSnapshot{};
        begin_attempt(&state, relative_path);
    }
    g_variant = variant;

    if (files_dir.empty()) {
        state.snapshot.status = LoadStatus::kFilesDirUnconfigured;
        return false;
    }
    const std::filesystem::path relative(relative_path);
    if (!safe_relative_path(relative)) {
        state.snapshot.status = LoadStatus::kInvalidRelativePath;
        return false;
    }

    std::error_code error;
    const std::filesystem::path root = std::filesystem::canonical(
            std::filesystem::path(files_dir) / "assets", error);
    if (error || !std::filesystem::is_directory(root, error) || error) {
        state.snapshot.status = LoadStatus::kAssetRootUnavailable;
        return false;
    }
    error.clear();
    const std::filesystem::path candidate = std::filesystem::weakly_canonical(root / relative, error);
    if (error) {
        state.snapshot.status = LoadStatus::kMissing;
        return false;
    }
    const std::string root_prefix = root.string() + std::filesystem::path::preferred_separator;
    const std::string candidate_string = candidate.string();
    if (candidate_string != root.string() && candidate_string.rfind(root_prefix, 0u) != 0u) {
        state.snapshot.status = LoadStatus::kPathEscapesAssetRoot;
        return false;
    }
    if (!std::filesystem::exists(candidate, error) || error) {
        state.snapshot.status = LoadStatus::kMissing;
        return false;
    }
    error.clear();
    if (!std::filesystem::is_regular_file(candidate, error) || error) {
        state.snapshot.status = LoadStatus::kNotRegularFile;
        return false;
    }
    error.clear();
    const auto raw_size = std::filesystem::file_size(candidate, error);
    if (error) {
        state.snapshot.status = LoadStatus::kReadFailed;
        return false;
    }
    state.snapshot.file_size = static_cast<std::uint64_t>(raw_size);
    if (raw_size > max_bytes) {
        state.snapshot.status = LoadStatus::kTooLarge;
        return false;
    }
    return parse_file(&state, candidate, static_cast<std::uint64_t>(raw_size));
}

Snapshot snapshot() {
    std::lock_guard<std::mutex> lock(g_mutex);
    Snapshot out;
    out.variant = g_variant;
    out.born = g_born.snapshot;
    out.idle = g_idle.snapshot;
    out.pair_ready = g_variant >= 1 && g_variant <= 4 &&
            out.born.status == LoadStatus::kReady &&
            out.idle.status == LoadStatus::kReady;
    return out;
}

std::optional<trap_actions_document::Document> current_document(Slot slot) {
    std::lock_guard<std::mutex> lock(g_mutex);
    return slot_state_const(slot).document;
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
    out << "Scene action pair documents\n";
    out << "variant: " << state.variant << "\n";
    out << "born: " << status_name(state.born.status) << "\n";
    out << "idle: " << status_name(state.idle.status) << "\n";
    out << "pair ready: " << (state.pair_ready ? "yes" : "no") << "\n";
    return out.str();
}

}  // namespace nevergone::scene_action_pair_document_store
