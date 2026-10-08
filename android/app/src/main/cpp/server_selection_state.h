#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

#include "login_callback_payload.h"

namespace nevergone::server_selection_state {

// Reconstructed semantic equivalent of the selection owned by NewServerList.
// The original confirms the selected ServerData as
// g_UILogin.EnterGameLogicServer(ip, id).
struct EnterRequest {
    bool valid = false;
    std::int64_t server_id = 0;
    std::string server_name;
    std::string ip;
};

struct Snapshot {
    bool payload_valid = false;
    std::uint64_t payload_generation = 0;
    std::size_t server_count = 0;
    int selected_index = -1;
    std::int64_t selected_server_id = 0;
    std::string selected_server_name;
    std::string selected_server_ip;
    std::string last_login_server;
    bool touch_active = false;
    int touch_pointer_id = -1;
    float touch_begin_y = 0.0f;
    std::uint64_t selection_changes = 0;
    std::uint64_t rejected_drag_touches = 0;
    std::uint64_t confirm_count = 0;
    bool enter_request_pending = false;
};

void reset();
void sync_server_list(const login_callback_payload::ServerListPayload& payload);

// NewServerList tags selectable row sprites 1..N. Project-owned compositors may
// work in zero-based indices and feed the resolved hit here.
bool select_index(int index);

// The recovered ccTouchEnded path only treats a gesture as a row tap when the
// absolute vertical movement from touch begin is <= 10 pixels.
void touch_began(int pointer_id, float y);
bool touch_ended(int pointer_id, float y, int hit_index);
void touch_cancelled(int pointer_id);

EnterRequest confirm_selection();
EnterRequest take_pending_enter_request();
Snapshot snapshot();
std::string status_report();

}  // namespace nevergone::server_selection_state
