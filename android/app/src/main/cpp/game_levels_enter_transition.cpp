#include "game_levels_enter_transition.h"

#include <mutex>
#include <sstream>

namespace nevergone::game_levels_enter_transition {
namespace {
std::mutex g_mutex;
Snapshot g_state;

Boundary classify(const game_levels_asset_probe::Snapshot& probe) {
    if (!probe.configured) return Boundary::kFilesDirUnconfigured;
    if (!probe.present) return Boundary::kAssetMissing;
    if (!probe.regular_file || !probe.within_size_limit || !probe.loaded) return Boundary::kAssetRejected;
    if (!probe.first_object_conditional_header_readable) return Boundary::kVerifiedPrefixIncomplete;
    return Boundary::kFirstObjectConditionalHeaderVerified;
}
}  // namespace

void reset() {
    std::lock_guard<std::mutex> lock(g_mutex);
    g_state = Snapshot{};
}

void on_enter_game(const std::string& files_dir) {
    on_enter_game_with_probe(game_levels_asset_probe::probe_pvp_scene(files_dir));
}

void on_enter_game_with_probe(const game_levels_asset_probe::Snapshot& probe) {
    std::lock_guard<std::mutex> lock(g_mutex);
    ++g_state.enter_callback_count;
    ++g_state.probe_attempt_count;
    g_state.boundary = classify(probe);
    g_state.file_size = probe.file_size;
    g_state.reader_size = probe.reader_size;
    g_state.verified_bytes = probe.first_object_conditional_header_readable
        ? probe.first_object_conditional_header_bytes_consumed
        : 0;
}

Snapshot snapshot() {
    std::lock_guard<std::mutex> lock(g_mutex);
    return g_state;
}

const char* boundary_name(Boundary boundary) {
    switch (boundary) {
        case Boundary::kIdle: return "idle";
        case Boundary::kFilesDirUnconfigured: return "files-dir-unconfigured";
        case Boundary::kAssetMissing: return "asset-missing";
        case Boundary::kAssetRejected: return "asset-rejected";
        case Boundary::kVerifiedPrefixIncomplete: return "verified-prefix-incomplete";
        case Boundary::kFirstObjectConditionalHeaderVerified:
            return "first-object-conditional-header-verified";
    }
    return "unknown";
}

std::string status_report() {
    const Snapshot state = snapshot();
    std::ostringstream out;
    out << "GameLevels enter boundary\n";
    out << "state: " << boundary_name(state.boundary) << "\n";
    out << "enter callbacks: " << state.enter_callback_count << "\n";
    out << "probe attempts: " << state.probe_attempt_count << "\n";
    if (state.reader_size != 0) out << "scene bytes loaded: " << state.reader_size << "\n";
    if (state.verified_bytes != 0) out << "verified LoadGL_Scene bytes: " << state.verified_bytes << "\n";
    return out.str();
}

}  // namespace nevergone::game_levels_enter_transition
