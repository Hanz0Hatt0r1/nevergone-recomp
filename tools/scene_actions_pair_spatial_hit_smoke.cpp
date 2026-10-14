#include <cassert>

#include "scene_actions_pair_spatial_hit.h"

int main() {
    namespace hit = nevergone::scene_actions_pair_spatial_hit;
    namespace binding = nevergone::scene_actions_spatial_pair_binding;
    namespace spatial = nevergone::scene_actions_spatial_trigger;

    binding::BoundObject source;
    source.ref = {2u, 1u};
    source.view.event_id_108 = 2004;
    source.view.resource_name = "source.png";
    source.view.object_position_x = 5.0f;
    source.view.sprite_bounds = spatial::Rect{10.0f, 20.0f, 20.0f, 20.0f};

    binding::BoundObject unresolved;
    unresolved.ref = {2u, 2u};
    unresolved.view.event_id_108 = 0;
    unresolved.view.resource_name = "trimmed-frame.png";
    // Deliberately no geometry: unrelated unresolved candidates must not block
    // the exact pair decision.

    binding::BoundObject pair;
    pair.ref = {2u, 3u};
    pair.view.resource_name = "klhuo-1.png";
    pair.view.object_position_x = 0.0f;
    pair.view.sprite_bounds = spatial::Rect{20.0f, 20.0f, 20.0f, 20.0f};

    const binding::ObjectRef pair_ref{2u, 3u};
    auto result = hit::evaluate(2004, pair_ref, {source, unresolved, pair});
    assert(result.status == hit::Status::kPairTriggered);
    assert(result.source_index.has_value() && *result.source_index == 0u);
    assert(result.pair_index.has_value() && *result.pair_index == 2u);
    assert(result.source_rect.has_value());
    assert(result.source_rect->x == 15.0f);

    // First matching source wins, preserving native traversal semantics.
    auto first_miss = source;
    first_miss.ref = {2u, 0u};
    first_miss.view.sprite_bounds = spatial::Rect{-100.0f, 20.0f, 10.0f, 10.0f};
    result = hit::evaluate(2004, pair_ref, {first_miss, source, pair});
    assert(result.status == hit::Status::kPairNotTriggered);
    assert(result.source_index.has_value() && *result.source_index == 0u);

    source.view.sprite_bounds.reset();
    result = hit::evaluate(2004, pair_ref, {source, pair});
    assert(result.status == hit::Status::kSourceGeometryUnavailable);

    source.view.sprite_bounds = spatial::Rect{10.0f, 20.0f, 20.0f, 20.0f};
    pair.view.sprite_bounds.reset();
    result = hit::evaluate(2004, pair_ref, {source, pair});
    assert(result.status == hit::Status::kPairGeometryUnavailable);

    result = hit::evaluate(1999, pair_ref, {source, pair});
    assert(result.status == hit::Status::kEventOutsideSceneActionRange);

    return 0;
}
