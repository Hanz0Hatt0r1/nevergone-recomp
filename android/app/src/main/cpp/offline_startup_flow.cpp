#include "offline_startup_flow.h"

#include <mutex>
#include <sstream>

namespace nevergone::offline_startup_flow {
namespace {

std::mutex g_mutex;
Snapshot g_state;

void resolve_route_if_possible_locked() {
    if (g_state.route != Route::kRoleProbePending ||
        !g_state.standalone_hero_presence_known) {
        return;
    }

    g_state.route = g_state.has_standalone_heroes
        ? Route::kChooseRole
        : Route::kOpeningDialogue;
    ++g_state.route_resolution_count;
}

}  // namespace

void reset() {
    std::lock_guard<std::mutex> lock(g_mutex);
    g_state = Snapshot{};
}

void set_standalone_hero_presence(bool has_standalone_heroes) {
    std::lock_guard<std::mutex> lock(g_mutex);
    g_state.standalone_hero_presence_known = true;
    g_state.has_standalone_heroes = has_standalone_heroes;
    resolve_route_if_possible_locked();
}

void on_auto_login_compat_success(std::uint64_t scene_generation) {
    if (scene_generation == 0) return;

    std::lock_guard<std::mutex> lock(g_mutex);
    if (g_state.scene_generation == scene_generation &&
        g_state.route != Route::kInactive) {
        return;
    }

    g_state.scene_generation = scene_generation;
    g_state.route = Route::kRoleProbePending;
    ++g_state.auto_login_success_count;
    resolve_route_if_possible_locked();
}

Snapshot snapshot() {
    std::lock_guard<std::mutex> lock(g_mutex);
    return g_state;
}

const char* route_name(Route route) {
    switch (route) {
        case Route::kInactive:
            return "inactive";
        case Route::kRoleProbePending:
            return "role-probe-pending";
        case Route::kOpeningDialogue:
            return "opening-dialogue";
        case Route::kChooseRole:
            return "choose-role";
    }
    return "unknown";
}

std::string status_report() {
    const Snapshot state = snapshot();
    std::ostringstream out;
    out << "offline startup route: " << route_name(state.route) << "\n";
    out << "offline startup generation: " << state.scene_generation << "\n";
    out << "standalone hero presence: ";
    if (!state.standalone_hero_presence_known) {
        out << "unknown\n";
    } else {
        out << (state.has_standalone_heroes ? "present" : "absent") << "\n";
    }
    out << "offline auto-login successes: " << state.auto_login_success_count << "\n";
    out << "offline role routes resolved: " << state.route_resolution_count << "\n";
    return out.str();
}

}  // namespace nevergone::offline_startup_flow
