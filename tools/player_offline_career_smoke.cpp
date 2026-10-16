#include <cassert>
#include <string>

#include "player_offline_career.h"

int main() {
    namespace career = nevergone::player_offline_career;

    nevergone::role_selection_state::Snapshot role;
    nevergone::character_name_state::Snapshot name;

    auto resolved = career::resolve(role, name);
    assert(!resolved.ready());
    assert(resolved.career == 0);
    assert(resolved.source == career::Source::kNone);

    // The fresh-role preservation path does not fabricate an EnterGame
    // dispatch. Once CharacterName completion records the successful local
    // handoff, its retained Career becomes the offline player index.
    name.active = false;
    name.completion_count = 1;
    name.career = 1;
    resolved = career::resolve(role, name);
    assert(resolved.ready());
    assert(resolved.career == 1);
    assert(resolved.clothes_id == 1);
    assert(resolved.source == career::Source::kFreshRoleCompletion);
    assert(std::string(career::source_name(resolved.source)) == "fresh-role-completion");

    // Merely opening/submitting CharacterName is insufficient: completion is
    // the recovered success/local-handoff boundary.
    name.active = true;
    resolved = career::resolve(role, name);
    assert(!resolved.ready());
    name.active = false;
    name.completion_count = 0;
    resolved = career::resolve(role, name);
    assert(!resolved.ready());

    // Existing-role dispatch remains authoritative when both retained states
    // are present in the same process lifetime.
    name.completion_count = 1;
    name.career = 1;
    role.enter_dispatch_committed = true;
    role.dispatched_career = 3;
    role.dispatched_clothes_id = 4;
    resolved = career::resolve(role, name);
    assert(resolved.ready());
    assert(resolved.career == 3);
    assert(resolved.clothes_id == 4);
    role.dispatched_clothes_id = 0;
    assert(career::resolve(role, name).clothes_id == -1);
    role.dispatched_clothes_id = 65536;
    assert(career::resolve(role, name).clothes_id == -1);
    assert(resolved.source == career::Source::kExistingRoleDispatch);
    assert(std::string(career::source_name(resolved.source)) == "existing-role-dispatch");

    role.dispatched_career = 9;
    name.career = 5;
    resolved = career::resolve(role, name);
    assert(resolved.ready());
    assert(resolved.career == 5);
    assert(resolved.source == career::Source::kFreshRoleCompletion);

    name.career = 0;
    resolved = career::resolve(role, name);
    assert(!resolved.ready());

    return 0;
}
