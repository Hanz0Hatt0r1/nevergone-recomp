#include "player_action_hitpoint_contract.h"

namespace nevergone::player_action_hitpoint_contract {

std::vector<RecordRef> records_for_frame(
        const enemy_actions_wbg_document::Document& document,
        std::int32_t current_frame) {
    std::vector<RecordRef> result;
    for (std::size_t index = 0;
         index < document.fixed_tail_block.records.size();
         ++index) {
        if (document.fixed_tail_block.records[index].first_i32 != current_frame) continue;
        result.push_back(RecordRef{index, result.size()});
    }
    return result;
}

std::optional<RecordRef> select_record(
        const enemy_actions_wbg_document::Document& document,
        std::int32_t current_frame,
        std::int32_t frame_ordinal) noexcept {
    if (frame_ordinal < 0) return std::nullopt;

    std::size_t ordinal = 0;
    for (std::size_t index = 0;
         index < document.fixed_tail_block.records.size();
         ++index) {
        if (document.fixed_tail_block.records[index].first_i32 != current_frame) continue;
        if (ordinal == static_cast<std::size_t>(frame_ordinal)) {
            return RecordRef{index, ordinal};
        }
        ++ordinal;
    }
    return std::nullopt;
}

}  // namespace nevergone::player_action_hitpoint_contract
