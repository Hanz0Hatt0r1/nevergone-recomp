#pragma once

#include <optional>
#include <string>

#include "enemy_actions_wbg_document.h"

namespace nevergone::tutorial_enemy_action_document_store {

struct Documents {
    unsigned enemy_id = 0;
    enemy_actions_wbg_document::Document appearance;
    enemy_actions_wbg_document::Document idle;
};

// Loads only action files selected by reviewed tutorial enemy mappings.
// Unsupported IDs and incomplete/corrupt pairs fail closed.
std::optional<Documents> current(unsigned enemy_id);
void reset();
std::string status_report();

}  // namespace nevergone::tutorial_enemy_action_document_store
