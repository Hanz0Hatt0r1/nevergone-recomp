#include <cassert>
#include <cstdint>
#include <vector>

#include "enemy_actions_default_cut_table.h"

namespace table = nevergone::enemy_actions_default_cut_table;
namespace document_ns = nevergone::enemy_actions_wbg_document;
namespace prefix_ns = nevergone::enemy_actions_wbg_prefix;

prefix_ns::ActionFrameRecord frame(
        const char* first_string,
        const char* third_string,
        std::int32_t third_length) {
    prefix_ns::ActionFrameRecord value;
    value.first_string = first_string;
    value.third_string = third_string;
    value.third_string_length_i32 = third_length;
    return value;
}

int main() {
    document_ns::Document document;
    document.primary_records.push_back(frame(
            "T_E01_default.plist", "empty_a01_001.png", 17));
    document.primary_records.push_back(frame(
            "T_E01_default.plist", "empty_a01_002.png", 17));
    document.primary_records.push_back(frame(
            "T_E01_default.plist", "empty_a01_001.png", 17));

    std::vector<table::SpriteFrameObservation> observations{
            {true, 64.0f},
            {true, 72.5f},
            {true, 80.0f},
    };

    const auto built = table::build(document, observations);
    assert(built.complete);
    assert(!built.stopped_on_unresolved_frame);
    assert(built.requested_capacity == 0x80u);
    assert(built.entries.size() == 3u);
    assert(built.entries[0].primary_index == 0u);
    assert(built.entries[0].frame_name_14 == "empty_a01_001.png");
    assert(built.entries[0].frame_name_length_6c == 17);
    assert(built.entries[0].rect_height_1c == 64.0f);
    assert(built.entries[1].rect_height_1c == 72.5f);

    // Native updateActionFrameMoveValue() scans from index zero, so duplicate
    // frame keys resolve to the first ActionsCut in system+0x27c.
    const auto duplicate = table::first_match(built.entries, "empty_a01_001.png");
    assert(duplicate.found);
    assert(duplicate.index == 0u);

    const auto second = table::first_match(built.entries, "empty_a01_002.png");
    assert(second.found);
    assert(second.index == 1u);

    const auto missing = table::first_match(built.entries, "missing.png");
    assert(!missing.found);

    // The clean-room builder stops safely where native would dereference a
    // missing CCSpriteFrame. It must not fabricate later ActionsCut entries.
    observations[1].resolved = false;
    const auto unresolved = table::build(document, observations);
    assert(!unresolved.complete);
    assert(unresolved.stopped_on_unresolved_frame);
    assert(unresolved.unresolved_primary_index == 1u);
    assert(unresolved.entries.size() == 1u);

    std::vector<table::SpriteFrameObservation> short_observations{{true, 12.0f}};
    const auto short_result = table::build(document, short_observations);
    assert(!short_result.complete);
    assert(short_result.unresolved_primary_index == 1u);
    assert(short_result.entries.size() == 1u);

    document_ns::Document empty;
    const auto empty_result = table::build(empty, {});
    assert(empty_result.complete);
    assert(empty_result.entries.empty());

    return 0;
}
