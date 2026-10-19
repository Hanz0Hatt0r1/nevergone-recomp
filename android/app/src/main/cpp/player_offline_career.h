#pragma once

#include <cstdint>
#include <sstream>
#include <string>

#include "character_name_state.h"
#include "game_scene_player_bootstrap.h"
#include "role_selection_state.h"

namespace nevergone::player_offline_career {

enum class Source {
    kNone = 0,
    kExistingRoleDispatch,
    kFreshRoleCompletion,
};

struct Resolution {
    std::int64_t career = 0;
    Source source = Source::kNone;
    // -1 means that no clothes selector was retained for this role.
    int clothes_id = -1;

    bool ready() const { return career >= 1 && career <= 5; }
};

inline bool valid_career(std::int64_t career) {
    return career >= 1 && career <= 5;
}

inline Resolution resolve(
        const role_selection_state::Snapshot& role,
        const character_name_state::Snapshot& character_name) {
    // Existing-role entry has the strongest runtime proof: this is the Career
    // that actually crossed the reconstructed EnterGameWithCid dispatch.
    if (role.enter_dispatch_committed && valid_career(role.dispatched_career)) {
        const auto clothes = role.dispatched_clothes_id;
        return {role.dispatched_career, Source::kExistingRoleDispatch,
                clothes > 0 && clothes <= 65535 ? static_cast<int>(clothes) : -1};
    }

    // Fresh-role preservation has no synthetic CharacterID/EnterGame dispatch.
    // CharacterNameLayer retains the exact Career passed by SingleSelectHero to
    // CretaUI(career). complete_creation() is reached only after the successful
    // CreateCharacter/local-GameLevels handoff, so that retained Career is the
    // evidence-backed offline player index for this serverless route.
    if (!character_name.active &&
            character_name.completion_count > 0u &&
            valid_career(character_name.career)) {
        return {character_name.career, Source::kFreshRoleCompletion,
                static_cast<int>(game_scene_player_bootstrap::kConstructionPlayerClothesId)};
    }

    return {};
}

inline Resolution current() {
    return resolve(role_selection_state::snapshot(), character_name_state::snapshot());
}

inline std::string current_player_name() {
    const auto role = role_selection_state::snapshot();
    if (role.enter_dispatch_committed && !role.dispatched_character_name.empty()) {
        return role.dispatched_character_name;
    }

    const auto created_role = character_name_state::snapshot();
    if (created_role.completion_count > 0u && !created_role.role_name.empty()) {
        return created_role.role_name;
    }

    return game_scene_player_bootstrap::player_name();
}

inline const char* source_name(Source source) {
    switch (source) {
        case Source::kNone: return "none";
        case Source::kExistingRoleDispatch: return "existing-role-dispatch";
        case Source::kFreshRoleCompletion: return "fresh-role-completion";
    }
    return "unknown";
}

inline std::string status_report() {
    const Resolution resolution = current();
    std::ostringstream out;
    out << "Offline player career\n";
    out << "state: " << (resolution.ready() ? "ready" : "idle") << "\n";
    out << "career: " << resolution.career << "\n";
    out << "clothes id: " << resolution.clothes_id << "\n";
    out << "source: " << source_name(resolution.source) << "\n";
    return out.str();
}

}  // namespace nevergone::player_offline_career
