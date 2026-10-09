#include "character_name_action_executor.h"

#include "character_random_name.h"
#include "login_lua_session.h"

namespace nevergone::character_name_action_executor {
namespace {

bool production_create_dispatch() {
    return login_lua_session::dispatch_pending_role_create_request();
}

bool production_randomize(std::string* error) {
    return character_random_name::fulfill_pending(error);
}

}  // namespace

Outcome dispatch_tag(int tag, std::string* error) {
    return dispatch_tag_with_callbacks(
        tag,
        &production_create_dispatch,
        &production_randomize,
        error);
}

}  // namespace nevergone::character_name_action_executor
