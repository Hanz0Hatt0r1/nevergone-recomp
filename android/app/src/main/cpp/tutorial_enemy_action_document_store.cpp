#include "tutorial_enemy_action_document_store.h"

#include <filesystem>
#include <fstream>
#include <cmath>
#include <mutex>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

#include "hp_data_reader.h"
#include "startup_contract.h"
#include "tutorial_enemy_action_selection.h"

namespace nevergone::tutorial_enemy_action_document_store {
namespace {

constexpr std::uint64_t kMaxActionBytes = 64u * 1024u * 1024u;
std::mutex g_mutex;
std::optional<Documents> g_documents;
unsigned g_attempted_enemy_id = 0;
std::uint64_t g_load_attempt_count = 0;
std::string g_status = "idle";
std::string g_appearance_path;
std::string g_idle_path;

bool safe_relative_path(const std::filesystem::path& path) {
    if (path.empty() || path.is_absolute()) return false;
    for (const auto& component : path) {
        if (component == "..") return false;
    }
    return true;
}

bool load_document(
        const std::filesystem::path& root,
        const char* relative_path,
        const char* expected_atlas,
        enemy_actions_wbg_document::Document* out) {
    if (relative_path == nullptr || expected_atlas == nullptr || out == nullptr) return false;
    const std::filesystem::path relative(relative_path);
    if (!safe_relative_path(relative)) return false;

    std::error_code error;
    const std::filesystem::path file =
            std::filesystem::weakly_canonical(root / relative, error);
    if (error) return false;
    const std::string root_prefix = root.string() + std::filesystem::path::preferred_separator;
    const std::string file_string = file.string();
    if (file_string.rfind(root_prefix, 0u) != 0u ||
        !std::filesystem::is_regular_file(file, error) || error) {
        return false;
    }

    error.clear();
    const auto file_size = std::filesystem::file_size(file, error);
    if (error || file_size == 0u || file_size > kMaxActionBytes) return false;

    std::ifstream input(file, std::ios::binary);
    if (!input) return false;
    std::vector<std::uint8_t> bytes(static_cast<std::size_t>(file_size));
    input.read(reinterpret_cast<char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    if (!input || input.gcount() != static_cast<std::streamsize>(bytes.size())) return false;

    hp_data::Reader reader(std::move(bytes));
    enemy_actions_wbg_document::Document parsed;
    if (!enemy_actions_wbg_document::parse(reader, &parsed) ||
        parsed.trailing_bytes != 0u || parsed.primary_records.empty() ||
        parsed.primary_records.size() != parsed.final_table.entries.size()) {
        return false;
    }

    for (std::size_t index = 0; index < parsed.primary_records.size(); ++index) {
        const auto& frame = parsed.primary_records[index];
        const float duration = parsed.final_table.entries[index].reciprocal_value;
        if (frame.first_string != expected_atlas || frame.third_string.empty() ||
            frame.third_string.find('/') != std::string::npos ||
            frame.third_string.find('\\') != std::string::npos ||
            frame.third_string.find("..") != std::string::npos ||
            !(duration > 0.0f) || !std::isfinite(duration)) {
            return false;
        }
    }

    *out = std::move(parsed);
    return true;
}

std::optional<Documents> load(unsigned enemy_id) {
    const auto* selection = tutorial_enemy_action_selection::for_enemy(enemy_id);
    if (selection == nullptr) return std::nullopt;

    const std::string& files_dir = startup::config().files_dir;
    if (files_dir.empty()) return std::nullopt;

    std::error_code error;
    const auto root = std::filesystem::canonical(
            std::filesystem::path(files_dir) / "assets", error);
    if (error || !std::filesystem::is_directory(root, error) || error) {
        return std::nullopt;
    }

    Documents documents;
    documents.enemy_id = enemy_id;
    if (!load_document(
                root,
                selection->start_action_data,
                selection->start_action_atlas,
                &documents.appearance) ||
        !load_document(
                root,
                selection->idle_action_data,
                selection->idle_action_atlas,
                &documents.idle)) {
        return std::nullopt;
    }
    return documents;
}

}  // namespace

std::optional<Documents> current(unsigned enemy_id) {
    std::lock_guard<std::mutex> lock(g_mutex);
    if (g_attempted_enemy_id != enemy_id) {
        g_attempted_enemy_id = enemy_id;
        ++g_load_attempt_count;
        const auto* selection = tutorial_enemy_action_selection::for_enemy(enemy_id);
        g_appearance_path = selection != nullptr ? selection->start_action_data : "";
        g_idle_path = selection != nullptr ? selection->idle_action_data : "";
        g_status = selection != nullptr ? "loading" : "unsupported-enemy";
        g_documents = load(enemy_id);
        g_status = g_documents.has_value() ? "ready" : "load-failed";
    }
    return g_documents;
}

void reset() {
    std::lock_guard<std::mutex> lock(g_mutex);
    g_documents.reset();
    g_attempted_enemy_id = 0;
    g_status = "idle";
    g_appearance_path.clear();
    g_idle_path.clear();
}

std::string status_report() {
    std::lock_guard<std::mutex> lock(g_mutex);
    std::string report = "Tutorial enemy action documents\n";
    report += "state: " + g_status + "\n";
    report += "enemy: " + std::to_string(g_attempted_enemy_id) + "\n";
    report += "load attempts: " + std::to_string(g_load_attempt_count) + "\n";
    if (!g_appearance_path.empty()) report += "appearance: " + g_appearance_path + "\n";
    if (!g_idle_path.empty()) report += "idle: " + g_idle_path + "\n";
    if (g_documents.has_value()) {
        report += "appearance frames: " +
                std::to_string(g_documents->appearance.primary_records.size()) + "\n";
        report += "idle frames: " +
                std::to_string(g_documents->idle.primary_records.size()) + "\n";
    }
    return report;
}

}  // namespace nevergone::tutorial_enemy_action_document_store
