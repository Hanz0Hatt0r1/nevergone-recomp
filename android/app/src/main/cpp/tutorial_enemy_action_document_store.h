#pragma once

#include <memory>
#include <string>

#include "enemy_actions_wbg_document.h"

namespace nevergone::tutorial_enemy_action_document_store {

struct Documents {
    unsigned enemy_id = 0;
    unsigned state_action_id = 0;
    bool state_action_loops = false;
    enemy_actions_wbg_document::Document appearance;
    enemy_actions_wbg_document::Document state_action;
};

// Loads only action files selected by reviewed enemy ID/state mappings.
// Unsupported states and incomplete/corrupt pairs fail closed.
std::shared_ptr<const Documents> current(unsigned enemy_id, int state_id);
void reset();
std::string status_report();

}  // namespace nevergone::tutorial_enemy_action_document_store
