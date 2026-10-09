#include "choose_hero_action_state.h"

#include <mutex>
#include <sstream>
#include <utility>

namespace nevergone::choose_hero_action_state {
namespace {

std::mutex g_mutex;
Snapshot g_state;

Action action_for_tag(int tag) {
    switch (tag) {
        case 1: return Action::kOpenCreateHeroScreen;
        case 2: return Action::kLeaveToSelectHero;
        case 3: return Action::kStartSelectedHero;
        case 4: return Action::kSubmitCreateRole;
        case 5: return Action::kRandomizeRoleName;
        case 6: return Action::kDetachImeAndRestoreSaveList;
        case 7: return Action::kAttachIme;
        case 8: return Action::kConfirmDeleteSelectedHero;
        default: return Action::kNone;
    }
}

}  // namespace

bool action_from_tag(int tag, Action* output) {
    if (output == nullptr) return false;
    const Action action = action_for_tag(tag);
    if (action == Action::kNone) {
        *output = Action::kNone;
        return false;
    }
    *output = action;
    return true;
}

const char* action_name(Action action) {
    switch (action) {
        case Action::kOpenCreateHeroScreen: return "open-create-hero-screen";
        case Action::kLeaveToSelectHero: return "leave-to-select-hero";
        case Action::kStartSelectedHero: return "start-selected-hero";
        case Action::kSubmitCreateRole: return "submit-create-role";
        case Action::kRandomizeRoleName: return "randomize-role-name";
        case Action::kDetachImeAndRestoreSaveList: return "detach-ime-restore-save-list";
        case Action::kAttachIme: return "attach-ime";
        case Action::kConfirmDeleteSelectedHero: return "confirm-delete-selected-hero";
        case Action::kNone:
        default: return "none";
    }
}

bool dispatch(
        int tag,
        std::uint64_t scene_generation,
        std::uint32_t selected_hero_id,
        std::int32_t create_role_parameter,
        std::string role_name) {
    const Action action = action_for_tag(tag);
    if (action == Action::kNone) return false;

    std::lock_guard<std::mutex> lock(g_mutex);
    if (g_state.scene_generation != scene_generation) {
        g_state = {};
        g_state.scene_generation = scene_generation;
    }

    ++g_state.dispatch_count;
    Request request;
    request.action = action;
    request.scene_generation = scene_generation;
    request.sequence = g_state.dispatch_count;
    request.selected_hero_id = selected_hero_id;
    request.create_role_parameter = create_role_parameter;
    request.role_name = std::move(role_name);
    g_state.latest = std::move(request);
    g_state.pending = true;
    return true;
}

bool peek(Request* output) {
    if (output == nullptr) return false;
    std::lock_guard<std::mutex> lock(g_mutex);
    if (!g_state.pending) {
        *output = {};
        return false;
    }
    *output = g_state.latest;
    return true;
}

bool consume(Request* output) {
    if (output == nullptr) return false;
    std::lock_guard<std::mutex> lock(g_mutex);
    if (!g_state.pending) {
        *output = {};
        return false;
    }
    *output = g_state.latest;
    g_state.pending = false;
    ++g_state.consume_count;
    return true;
}

void reset(std::uint64_t scene_generation) {
    std::lock_guard<std::mutex> lock(g_mutex);
    g_state = {};
    g_state.scene_generation = scene_generation;
}

Snapshot snapshot() {
    std::lock_guard<std::mutex> lock(g_mutex);
    return g_state;
}

std::string status_report() {
    const Snapshot state = snapshot();
    std::ostringstream out;
    out << "ChooseHero actions: generation=" << state.scene_generation
        << " dispatched=" << state.dispatch_count
        << " consumed=" << state.consume_count
        << " pending=" << (state.pending ? "yes" : "no") << "\n";
    out << "ChooseHero latest action: " << action_name(state.latest.action)
        << " sequence=" << state.latest.sequence
        << " selected=" << state.latest.selected_hero_id
        << " create-param=" << state.latest.create_role_parameter
        << " name-bytes=" << state.latest.role_name.size() << "\n";
    return out.str();
}

}  // namespace nevergone::choose_hero_action_state
