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
    assert(state.selected_server_id == 8);
    assert(state.selected_server_name == "Asia");
    assert(state.selected_server_ip == "10.0.1.1");

    // The recovered Tuijian path only preselects an exact LastLoginServer id.
    sync_server_list(sample_payload("999"));
    state = snapshot();
    assert(state.selected_index == -1);
    sync_server_list(sample_payload("8x"));
    assert(snapshot().selected_index == -1);
    sync_server_list(sample_payload(""));
    assert(snapshot().selected_index == -1);

    sync_server_list(sample_payload("7"));
    assert(snapshot().selected_index == 0);

    // Exactly 10 vertical pixels is still a tap in the recovered ccTouchEnded.
    touch_began(3, 100.0f);
    assert(touch_ended(3, 110.0f, 2));
    assert(snapshot().selected_index == 2);

    // More than 10 pixels is scrolling/dragging and must not alter selection.
    touch_began(3, 100.0f);
    assert(!touch_ended(3, 110.01f, 1));
    state = snapshot();
    assert(state.selected_index == 2);
    assert(state.rejected_drag_touches == 1);

    // A different pointer cannot finish the active row tap.
    touch_began(4, 50.0f);
    assert(!touch_ended(5, 50.0f, 1));
    assert(snapshot().touch_active);
    touch_cancelled(4);
    assert(!snapshot().touch_active);

    // No resolved row hit is not a selection.
    touch_began(6, 70.0f);
    assert(!touch_ended(6, 72.0f, -1));
    assert(snapshot().selected_index == 2);

    const EnterRequest request = confirm_selection();
    assert(request.valid);
    assert(request.server_id == 11);
    assert(request.server_name == "America");
    assert(request.ip == "10.0.2.1");
    assert(snapshot().enter_request_pending);
    assert(snapshot().confirm_count == 1);

    const EnterRequest pending = take_pending_enter_request();
    assert(pending.valid);
    assert(pending.server_id == 11);
    assert(pending.ip == "10.0.2.1");
    assert(!take_pending_enter_request().valid);
    assert(!snapshot().enter_request_pending);

    // A new server payload invalidates old pending actions and applies its own
    // LastLoginServer recommendation.
    assert(select_index(1));
    assert(confirm_selection().valid);
    sync_server_list(sample_payload("11"));
    state = snapshot();
    assert(state.selected_index == 2);
    assert(state.selected_server_id == 11);
    assert(!state.enter_request_pending);

    // Invalid payloads are inert and cannot produce an EnterGame request.
    nevergone::login_callback_payload::ServerListPayload invalid;
    sync_server_list(invalid);
    assert(!snapshot().payload_valid);
    assert(snapshot().selected_index == -1);
    assert(!confirm_selection().valid);

    return 0;
}
