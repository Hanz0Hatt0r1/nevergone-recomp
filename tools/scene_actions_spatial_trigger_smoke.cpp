#include <cassert>
#include <cmath>
#include <limits>
#include <vector>

#include "scene_actions_spatial_trigger.h"

int main() {
    namespace spatial = nevergone::scene_actions_spatial_trigger;

    // Edge-touching rectangles intersect in the shipped cocos2d implementation.
    assert(spatial::intersects({0.0f, 0.0f, 10.0f, 10.0f},
                               {10.0f, 2.0f, 5.0f, 5.0f}));
    assert(!spatial::intersects({0.0f, 0.0f, 10.0f, 10.0f},
                                {10.01f, 2.0f, 5.0f, 5.0f}));

    spatial::ObjectView source;
    source.event_id_108 = 2001;
    source.resource_name = "source.png";
    source.object_position_x = 100.0f;
    source.sprite_bounds = spatial::Rect{5.0f, 20.0f, 20.0f, 20.0f};

    // Candidate local bbox is translated only by GameSceneObject position X.
    spatial::ObjectView hit;
    hit.resource_name = "ordinary.png";
    hit.object_position_x = 110.0f;
    hit.sprite_bounds = spatial::Rect{15.0f, 20.0f, 10.0f, 10.0f};

    spatial::ObjectView miss = hit;
    miss.object_position_x = 500.0f;

    spatial::ObjectView no_sprite;
    no_sprite.resource_name = "ordinary.png";

    spatial::ObjectView excluded = hit;
    excluded.resource_name = "czyanwu1.png";

    // A second object with the same event id must not replace the first source.
    spatial::ObjectView later_source = source;
    later_source.object_position_x = 900.0f;

    std::vector<spatial::ObjectView> objects{
        source, hit, miss, no_sprite, excluded, later_source,
    };
    auto result = spatial::dispatch(2001, objects);
    assert(result.status == spatial::Status::kReady);
    assert(result.source_index.has_value() && *result.source_index == 0u);
    assert(result.source_rect.has_value());
    assert(std::fabs(result.source_rect->x - 105.0f) < 0.0001f);
    assert(std::fabs(result.source_rect->y - 20.0f) < 0.0001f);
    // The source intersects itself. hit touches/overlaps. miss/no-sprite/fog and
    // the far duplicate source are not dispatched.
    assert(result.trigger_indexes.size() == 2u);
    assert(result.trigger_indexes[0] == 0u);
    assert(result.trigger_indexes[1] == 1u);

    // getObjectRect() has an exact 1x1 fallback when the *source* has no sprite.
    std::vector<spatial::ObjectView> fallback_source(2);
    fallback_source[0].event_id_108 = 2002;
    fallback_source[0].resource_name = "source-without-sprite";
    fallback_source[0].object_position_x = 999.0f;  // ignored without sprite
    fallback_source[1].resource_name = "candidate.png";
    fallback_source[1].sprite_bounds = spatial::Rect{0.0f, 0.0f, 1.0f, 1.0f};
    result = spatial::dispatch(2002, fallback_source);
    assert(result.status == spatial::Status::kReady);
    assert(result.source_rect->x == 0.0f && result.source_rect->y == 0.0f);
    assert(result.source_rect->width == 1.0f && result.source_rect->height == 1.0f);
    assert(result.trigger_indexes.size() == 1u && result.trigger_indexes[0] == 1u);

    // Both fog names bypass intersection even when their geometry overlaps.
    excluded.resource_name = "czyanwu3.png";
    auto contains = spatial::contains_rect(excluded, {0.0f, 0.0f, 1000.0f, 1000.0f});
    assert(contains.has_value() && !*contains);

    result = spatial::dispatch(2000, objects);
    assert(result.status == spatial::Status::kEventOutsideSceneActionRange);
    result = spatial::dispatch(2020, objects);
    assert(result.status == spatial::Status::kEventOutsideSceneActionRange);
    result = spatial::dispatch(2005, objects);
    assert(result.status == spatial::Status::kSourceNotFound);

    auto invalid = objects;
    invalid[1].object_position_x = std::numeric_limits<float>::quiet_NaN();
    result = spatial::dispatch(2001, invalid);
    assert(result.status == spatial::Status::kInvalidGeometry);

    return 0;
}
