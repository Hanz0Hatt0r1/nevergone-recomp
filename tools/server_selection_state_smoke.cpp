#include <cassert>

#include "server_selection_state.h"

namespace {

nevergone::login_callback_payload::ServerListPayload sample_payload(
        const std::string& last_login_server) {
    using nevergone::login_callback_payload::ServerEntry;
    using nevergone::login_callback_payload::ServerListPayload;
    ServerListPayload payload;
    payload.valid = true;
    payload.last_login_server = last_login_server;
    payload.servers = {
        ServerEntry{7, "Europe", "10.0.0.1", "10.0.0.2"},
        ServerEntry{8, "Asia", "10.0.1.1", ""},
        ServerEntry{11, "America", "10.0.2.1", ""},
    };
    return payload;
}

}  // namespace

int main() {
    using namespace nevergone::server_selection_state;

    reset();
    sync_server_list(sample_payload("8"));
    auto state = snapshot();
    assert(state.payload_valid);
    assert(state.server_count == 3);
    assert(state.selected_index == 1);
    assert(!state.chooser_open);
    assert(state.selected_server_id == 8);
    assert(state.selected_server_name == "Asia");
    assert(state.selected_server_ip == "10.0.1.1");

    // Original tag 10001 opens the hidden server list. Repeated opens are
    // idempotent; only the closed->open transition is counted.
    assert(open_chooser());
    state = snapshot();
    assert(state.chooser_open);
    assert(state.chooser_open_count == 1);
    assert(open_chooser());
    assert(snapshot().chooser_open_count == 1);

    touch_began(3, 100.0f);
    assert(touch_ended(3, 110.0f, 2));
    state = snapshot();
    assert(state.selected_index == 2);
    assert(!state.chooser_open);

    // Rows are not selectable while the original overlay is closed.
    touch_began(3, 100.0f);
    assert(!touch_ended(3, 100.0f, 1));
    assert(snapshot().selected_index == 2);

    assert(open_chooser());
    touch_began(3, 100.0f);
    assert(!touch_ended(3, 110.01f, 1));
    state = snapshot();
    assert(state.selected_index == 2);
    assert(state.chooser_open);
    assert(state.rejected_drag_touches == 1);

    touch_began(4, 50.0f);
    assert(!touch_ended(5, 50.0f, 1));
    assert(snapshot().touch_active);
    touch_cancelled(4);
    assert(!snapshot().touch_active);

    touch_began(6, 70.0f);
    assert(!touch_ended(6, 72.0f, -1));
    assert(snapshot().selected_index == 2);
    assert(snapshot().chooser_open);

    // Confirm is disabled while the modal chooser is open.
    assert(!confirm_selection().valid);
    assert(!snapshot().enter_request_pending);

    // Selecting a valid row mirrors UpDataServerSelet: close overlay and
    // reactivate the main confirm path.
    assert(select_index(2));
    assert(!snapshot().chooser_open);
    const EnterRequest request = confirm_selection();
    assert(request.valid);
    assert(request.server_id == 11);
    assert(request.server_name == "America");
    assert(request.ip == "10.0.2.1");
    assert(snapshot().enter_request_pending);
    assert(snapshot().confirm_count == 1);

    const EnterRequest peeked = peek_pending_enter_request();
    assert(peeked.valid);
    assert(peeked.server_id == 11);
    assert(peeked.ip == "10.0.2.1");
    assert(snapshot().enter_request_pending);

    const EnterRequest pending = take_pending_enter_request();
    assert(pending.valid);
    assert(pending.server_id == 11);
    assert(pending.ip == "10.0.2.1");
    assert(!peek_pending_enter_request().valid);
    assert(!take_pending_enter_request().valid);
    assert(!snapshot().enter_request_pending);

    assert(select_index(1));
    assert(confirm_selection().valid);
    sync_server_list(sample_payload("11"));
    state = snapshot();
    assert(state.selected_index == 2);
    assert(state.selected_server_id == 11);
    assert(!state.chooser_open);
    assert(!state.enter_request_pending);

    sync_server_list(sample_payload("999"));
    state = snapshot();
    assert(state.selected_index == -1);
    assert(!state.chooser_open);
    assert(open_chooser());
    assert(snapshot().chooser_open);

    sync_server_list(sample_payload("8x"));
    assert(snapshot().selected_index == -1);
    assert(!snapshot().chooser_open);
    sync_server_list(sample_payload(""));
    assert(snapshot().selected_index == -1);

    nevergone::login_callback_payload::ServerListPayload invalid;
    sync_server_list(invalid);
    assert(!snapshot().payload_valid);
    assert(snapshot().selected_index == -1);
    assert(!snapshot().chooser_open);
    assert(!open_chooser());
    assert(!confirm_selection().valid);

    return 0;
}
