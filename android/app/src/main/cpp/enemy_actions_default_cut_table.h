#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "enemy_actions_wbg_document.h"

namespace nevergone::enemy_actions_default_cut_table {

// Native initResDefaultRectArray() creates system+0x27c with capacity 0x80.
constexpr std::size_t kNativeRequestedCapacity = 0x80u;

// Project-owned observation of the CCSpriteFrame returned for one primary
// ActionFrameData+0x68 lookup. Native dereferences the frame unconditionally;
// the clean-room layer instead stops safely when a lookup was unresolved.
struct SpriteFrameObservation {
    bool resolved = false;
    float rect_height = 0.0f;
};

// Offset-faithful value stored by the native ActionsCut object created for the
// system+0x27c default table.
struct Entry {
    std::size_t primary_index = 0;
    std::string frame_name_14;
    std::int32_t frame_name_length_6c = 0;
    float rect_height_1c = 0.0f;
};

struct BuildResult {
    std::size_t requested_capacity = kNativeRequestedCapacity;
    std::vector<Entry> entries;
    bool complete = true;
    bool stopped_on_unresolved_frame = false;
    std::size_t unresolved_primary_index = 0;
};

inline BuildResult build(
        const enemy_actions_wbg_document::Document& document,
        const std::vector<SpriteFrameObservation>& observations) {
    BuildResult result;
    result.entries.reserve(document.primary_records.size());

    for (std::size_t i = 0; i < document.primary_records.size(); ++i) {
        if (i >= observations.size() || !observations[i].resolved) {
            result.complete = false;
            result.stopped_on_unresolved_frame = true;
            result.unresolved_primary_index = i;
            return result;
        }

        const auto& frame = document.primary_records[i];
        Entry entry;
        entry.primary_index = i;
        entry.frame_name_14 = frame.third_string;
        entry.frame_name_length_6c = frame.third_string_length_i32;
        entry.rect_height_1c = observations[i].rect_height;
        result.entries.push_back(entry);
    }

    return result;
}

struct MatchResult {
    bool found = false;
    std::size_t index = 0;
};

// updateActionFrameMoveValue() walks system+0x27c from index zero and compares
// ActionsCut+0x14 with current ActionFrameData+0x68, so duplicate keys select
// the first entry.
inline MatchResult first_match(
        const std::vector<Entry>& entries,
        const std::string& frame_name_68) {
    for (std::size_t i = 0; i < entries.size(); ++i) {
        if (entries[i].frame_name_14 == frame_name_68) return {true, i};
    }
    return {};
}

}  // namespace nevergone::enemy_actions_default_cut_table
