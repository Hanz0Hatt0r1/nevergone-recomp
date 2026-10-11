#pragma once

#include <cstddef>
#include <cstdint>
#include <map>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include "game_scene_construction_plan.h"

namespace nevergone::game_scene_construction_diagnostics {

constexpr std::size_t kMaxReportedUnresolvedTypeCodes = 16u;

struct TypeCount {
    std::int32_t type_code = 0;
    std::size_t count = 0;
};

struct UnresolvedTypeSummary {
    std::size_t object_count = 0;
    std::size_t distinct_type_count = 0;
    std::vector<TypeCount> reported_types;
};

inline UnresolvedTypeSummary summarize_unresolved_types(
        const game_scene_construction_plan::ScenePlan& plan) {
    std::map<std::int32_t, std::size_t> counts;
    UnresolvedTypeSummary summary;

    for (const auto& layer : plan.layers) {
        for (const auto& object : layer.objects) {
            if (object.construction_kind !=
                    game_scene_construction_plan::ObjectConstructionKind::kUnresolved) {
                continue;
            }
            ++summary.object_count;
            ++counts[object.type_code];
        }
    }

    summary.distinct_type_count = counts.size();
    summary.reported_types.reserve(
            counts.size() < kMaxReportedUnresolvedTypeCodes
                    ? counts.size()
                    : kMaxReportedUnresolvedTypeCodes);
    for (const auto& [type_code, count] : counts) {
        if (summary.reported_types.size() >= kMaxReportedUnresolvedTypeCodes) break;
        summary.reported_types.push_back(TypeCount{type_code, count});
    }
    return summary;
}

inline std::string format_unresolved_types(const UnresolvedTypeSummary& summary) {
    if (summary.object_count == 0u || summary.distinct_type_count == 0u) return {};

    std::ostringstream out;
    for (std::size_t index = 0; index < summary.reported_types.size(); ++index) {
        if (index != 0u) out << ',';
        out << summary.reported_types[index].type_code
            << '=' << summary.reported_types[index].count;
    }
    if (summary.distinct_type_count > summary.reported_types.size()) {
        out << ",+"
            << (summary.distinct_type_count - summary.reported_types.size())
            << "-types";
    }
    return out.str();
}

inline std::string unresolved_type_report(
        const game_scene_construction_plan::ScenePlan& plan) {
    return format_unresolved_types(summarize_unresolved_types(plan));
}

}  // namespace nevergone::game_scene_construction_diagnostics
