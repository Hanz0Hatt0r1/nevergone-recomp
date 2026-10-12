#pragma once

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace nevergone::scene_actions_spatial_trigger {

constexpr std::int32_t kFirstSceneActionEventId = 2001;
constexpr std::int32_t kLastSceneActionEventId = 2019;

struct Rect {
    float x = 0.0f;
    float y = 0.0f;
    float width = 0.0f;
    float height = 0.0f;
};

// Renderer-independent view of the fields consumed by the recovered native
// getObjectRect()/isContainsRect() path. `sprite_bounds` is the result of the
// original CCSprite::sprBoundingBox() call before GameSceneObject position-X is
// added. A missing sprite corresponds to GameSceneObject +0x10c == nullptr.
struct ObjectView {
    std::int32_t event_id_108 = 0;
    std::string resource_name;
    float object_position_x = 0.0f;
    std::optional<Rect> sprite_bounds;
};

enum class Status {
    kReady = 0,
    kEventOutsideSceneActionRange,
    kSourceNotFound,
    kInvalidGeometry,
};

struct Result {
    Status status = Status::kEventOutsideSceneActionRange;
    std::optional<std::size_t> source_index;
    std::optional<Rect> source_rect;
    // Ordered indexes whose native GameSceneObject::triggerSceneAction() call
    // would be reached by showEventObjTip().
    std::vector<std::size_t> trigger_indexes;
};

inline bool finite_rect(const Rect& rect) {
    return std::isfinite(rect.x) && std::isfinite(rect.y) &&
            std::isfinite(rect.width) && std::isfinite(rect.height);
}

inline bool finite_object(const ObjectView& object) {
    return std::isfinite(object.object_position_x) &&
            (!object.sprite_bounds.has_value() || finite_rect(*object.sprite_bounds));
}

// Exact cocos2d::CCRect::intersectsRect edge policy recovered from the shipped
// binary: touching edges intersect. Separation requires one max coordinate to
// be strictly smaller than the opposite min coordinate.
inline bool intersects(const Rect& lhs, const Rect& rhs) {
    if (!finite_rect(lhs) || !finite_rect(rhs)) return false;
    const float lhs_max_x = lhs.x + lhs.width;
    const float rhs_max_x = rhs.x + rhs.width;
    const float lhs_max_y = lhs.y + lhs.height;
    const float rhs_max_y = rhs.y + rhs.height;
    if (!std::isfinite(lhs_max_x) || !std::isfinite(rhs_max_x) ||
            !std::isfinite(lhs_max_y) || !std::isfinite(rhs_max_y)) {
        return false;
    }
    return !(lhs_max_x < rhs.x || rhs_max_x < lhs.x ||
            lhs_max_y < rhs.y || rhs_max_y < lhs.y);
}

// GameSceneObject::getObjectRect(): when +0x10c is absent the original returns
// CCRect(0, 0, 1, 1). Otherwise it takes sprBoundingBox() and adds only the
// GameSceneObject node's getPositionX() to the returned origin.x.
inline std::optional<Rect> object_rect(const ObjectView& object) {
    if (!finite_object(object)) return std::nullopt;
    if (!object.sprite_bounds.has_value()) return Rect{0.0f, 0.0f, 1.0f, 1.0f};
    Rect rect = *object.sprite_bounds;
    rect.x += object.object_position_x;
    if (!finite_rect(rect)) return std::nullopt;
    return rect;
}

inline bool excluded_from_contains(const std::string& resource_name) {
    return resource_name == "czyanwu1.png" || resource_name == "czyanwu3.png";
}

// GameSceneObject::isContainsRect(CCRect): no sprite and the two recovered fog
// resources return false immediately. For every other resource, intersection is
// tested after the same position-X adjustment used by getObjectRect(). Several
// light resources perform visual fade side effects after a successful hit, but
// all of those paths still return true; those decorative effects are outside
// this predicate contract.
inline std::optional<bool> contains_rect(const ObjectView& candidate, const Rect& target) {
    if (!finite_object(candidate) || !finite_rect(target)) return std::nullopt;
    if (!candidate.sprite_bounds.has_value() || excluded_from_contains(candidate.resource_name)) {
        return false;
    }
    const auto candidate_rect = object_rect(candidate);
    if (!candidate_rect.has_value()) return std::nullopt;
    return intersects(*candidate_rect, target);
}

// Reconstruct the event-2001..2019 branch of
// GameSceneClassLayer::showEventObjTip(...):
// 1. find the first layer object whose +0x108 equals event_id;
// 2. build that source object's getObjectRect();
// 3. scan every layer object in array order;
// 4. invoke triggerSceneAction() for each object whose isContainsRect() is true.
//
// This function returns the invocation indexes only. It deliberately does not
// perform triggerSceneAction() itself, choose a SceneActions variant, or model
// the separate showEventObjTip branches used by non-2001..2019 events.
inline Result dispatch(std::int32_t event_id, const std::vector<ObjectView>& objects) {
    Result result;
    if (event_id < kFirstSceneActionEventId || event_id > kLastSceneActionEventId) {
        result.status = Status::kEventOutsideSceneActionRange;
        return result;
    }

    std::size_t source = objects.size();
    for (std::size_t index = 0; index < objects.size(); ++index) {
        if (!finite_object(objects[index])) {
            result.status = Status::kInvalidGeometry;
            return result;
        }
        if (source == objects.size() && objects[index].event_id_108 == event_id) {
            source = index;
        }
    }
    if (source == objects.size()) {
        result.status = Status::kSourceNotFound;
        return result;
    }

    const auto source_rect = object_rect(objects[source]);
    if (!source_rect.has_value()) {
        result.status = Status::kInvalidGeometry;
        return result;
    }
    result.source_index = source;
    result.source_rect = *source_rect;

    for (std::size_t index = 0; index < objects.size(); ++index) {
        const auto hit = contains_rect(objects[index], *source_rect);
        if (!hit.has_value()) {
            result.status = Status::kInvalidGeometry;
            result.trigger_indexes.clear();
            return result;
        }
        if (*hit) result.trigger_indexes.push_back(index);
    }
    result.status = Status::kReady;
    return result;
}

}  // namespace nevergone::scene_actions_spatial_trigger
