#include "character_name_action_executor.h"

#include <utility>

#include "character_name_state.h"
#include "character_random_name.h"
#include "login_lua_session.h"
#include "role_creation_validation.h"
#include "role_selection_state.h"

namespace nevergone::character_name_action_executor {
namespace {

void set_error(std::string* error, std::string value) {
    if (error != nullptr) *error = std::move(value);
}

bool production_create_dispatch() {
    return login_lua_session::dispatch_pending_role_create_request();
}

bool production_randomize(std::string* error) {
    return character_random_name::fulfill_pending(error);
}

}  // namespace

Outcome dispatch_tag_with_callbacks(
        int tag,
        CreateDispatchFn create_dispatch,
        RandomizeFn randomize,
        std::string* error) {
    character_name_state::Action action = character_name_state::Action::kNone;
    if (!character_name_state::action_from_tag(tag, &action)) {
        set_error(error, "unsupported CharacterNameLayer tag");
        return Outcome::kInvalidTag;
    }

    const auto before = character_name_state::snapshot();
    if (!before.active) {
        set_error(error, "CharacterNameLayer is inactive");
        return Outcome::kInactive;
    }

    if (!character_name_state::dispatch_tag(tag)) {
        set_error(error, "CharacterNameLayer rejected action dispatch");
        return Outcome::kInactive;
    }

    switch (action) {
        case character_name_state::Action::kSubmit: {
            const auto after = character_name_state::snapshot();
            if (after.last_validation.result != role_creation_validation::Result::kValid) {
                set_error(
                    error,
                    std::string("character name validation rejected: ") +
                        role_creation_validation::result_name(after.last_validation.result));
                return Outcome::kValidationRejected;
            }

            const auto pending = role_selection_state::peek_pending_create_request();
            if (!pending.valid || pending.career != after.career ||
                    pending.character_name != after.role_name) {
                set_error(error, "CharacterNameLayer create request was not staged");
                return Outcome::kCreateRequestMissing;
            }

            if (create_dispatch == nullptr || !create_dispatch()) {
                set_error(error, "CreateCharacter dispatch failed");
                return Outcome::kCreateDispatchFailed;
            }

            if (error != nullptr) error->clear();
            return Outcome::kSubmitted;
        }

        case character_name_state::Action::kClose:
            if (error != nullptr) error->clear();
            return Outcome::kClosed;

        case character_name_state::Action::kRandomize:
            if (randomize == nullptr || !randomize(error)) {
                if (error != nullptr && error->empty()) {
                    *error = "CharacterNameLayer random-name dispatch failed";
                }
                return Outcome::kRandomizeFailed;
            }
            if (error != nullptr) error->clear();
            return Outcome::kRandomized;

        case character_name_state::Action::kNone:
            break;
    }

    set_error(error, "CharacterNameLayer action has no executor");
    return Outcome::kInvalidTag;
}

Outcome dispatch_tag(int tag, std::string* error) {
    return dispatch_tag_with_callbacks(
        tag,
        &production_create_dispatch,
        &production_randomize,
        error);
}

const char* outcome_name(Outcome outcome) {
    switch (outcome) {
        case Outcome::kInvalidTag: return "invalid-tag";
        case Outcome::kInactive: return "inactive";
        case Outcome::kValidationRejected: return "validation-rejected";
        case Outcome::kCreateRequestMissing: return "create-request-missing";
        case Outcome::kCreateDispatchFailed: return "create-dispatch-failed";
        case Outcome::kSubmitted: return "submitted";
        case Outcome::kClosed: return "closed";
        case Outcome::kRandomizeFailed: return "randomize-failed";
        case Outcome::kRandomized: return "randomized";
    }
    return "unknown";
}

}  // namespace nevergone::character_name_action_executor
