#include <cassert>
#include <cstdint>
#include <string>

#include "game_scene_construction_diagnostics.h"

int main() {
    namespace construction = nevergone::game_scene_construction_plan;
    namespace diagnostics = nevergone::game_scene_construction_diagnostics;

    construction::ScenePlan plan;
    construction::LayerPlan layer;

    construction::ObjectPlan type6a;
    type6a.type_code = 6;
    type6a.construction_kind = construction::ObjectConstructionKind::kUnresolved;
    layer.objects.push_back(type6a);

    construction::ObjectPlan type2;
    type2.type_code = 2;
    type2.construction_kind = construction::ObjectConstructionKind::kUnresolved;
    layer.objects.push_back(type2);

    construction::ObjectPlan type6b;
    type6b.type_code = 6;
    type6b.construction_kind = construction::ObjectConstructionKind::kUnresolved;
    layer.objects.push_back(type6b);

    construction::ObjectPlan resolvedSprite;
    resolvedSprite.type_code = 0;
    resolvedSprite.construction_kind = construction::ObjectConstructionKind::kType0SpriteBacked;
    layer.objects.push_back(resolvedSprite);

    construction::ObjectPlan deferredAction;
    deferredAction.type_code = 0;
    deferredAction.construction_kind = construction::ObjectConstructionKind::kType0SceneActionPair;
    layer.objects.push_back(deferredAction);

    plan.layers.push_back(layer);

    auto summary = diagnostics::summarize_unresolved_types(plan);
    assert(summary.object_count == 3u);
    assert(summary.distinct_type_count == 2u);
    assert(summary.reported_types.size() == 2u);
    assert(summary.reported_types[0].type_code == 2);
    assert(summary.reported_types[0].count == 1u);
    assert(summary.reported_types[1].type_code == 6);
    assert(summary.reported_types[1].count == 2u);
    assert(diagnostics::format_unresolved_types(summary) == "2=1,6=2");
    assert(diagnostics::unresolved_type_report(plan) == "2=1,6=2");

    construction::ScenePlan capped;
    construction::LayerPlan cappedLayer;
    for (std::int32_t typeCode = 1; typeCode <= 20; ++typeCode) {
        construction::ObjectPlan object;
        object.type_code = typeCode;
        object.construction_kind = construction::ObjectConstructionKind::kUnresolved;
        cappedLayer.objects.push_back(object);
    }
    capped.layers.push_back(cappedLayer);

    summary = diagnostics::summarize_unresolved_types(capped);
    assert(summary.object_count == 20u);
    assert(summary.distinct_type_count == 20u);
    assert(summary.reported_types.size() == diagnostics::kMaxReportedUnresolvedTypeCodes);
    assert(summary.reported_types.front().type_code == 1);
    assert(summary.reported_types.back().type_code == 16);
    const std::string cappedReport = diagnostics::format_unresolved_types(summary);
    assert(cappedReport.find("1=1") == 0u);
    assert(cappedReport.find("16=1") != std::string::npos);
    assert(cappedReport.find(",+4-types") != std::string::npos);
    assert(cappedReport.find("17=1") == std::string::npos);

    construction::ScenePlan empty;
    summary = diagnostics::summarize_unresolved_types(empty);
    assert(summary.object_count == 0u);
    assert(summary.distinct_type_count == 0u);
    assert(summary.reported_types.empty());
    assert(diagnostics::format_unresolved_types(summary).empty());

    return 0;
}
