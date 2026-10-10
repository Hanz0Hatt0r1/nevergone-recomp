#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "game_scene_render_queue.h"

namespace nevergone::game_scene_direct_asset_requests {

enum class Kind : std::uint8_t {
    kDirectFile = 0,
    kSpriteFrameByName = 1,
};

struct Request {
    Kind kind = Kind::kDirectFile;
    std::size_t sprite_command_index = 0;
    std::string relative_path;
    std::string frame_name;
};

struct Snapshot {
    std::size_t source_scene_index = 0;
    std::string guid;
    std::uint64_t revision = 0;
    std::vector<Request> requests;
};

namespace detail {
constexpr std::uint64_t kFnvOffsetBasis = 14695981039346656037ull;
constexpr std::uint64_t kFnvPrime = 1099511628211ull;
constexpr std::uint64_t kJavaLongPositiveMask = 0x7fffffffffffffffull;

inline void hash_byte(std::uint64_t* hash, std::uint8_t value) {
    *hash ^= value;
    *hash *= kFnvPrime;
}

inline void hash_size(std::uint64_t* hash, std::size_t value) {
    for (std::size_t shift = 0; shift < sizeof(value) * 8u; shift += 8u) {
        hash_byte(hash, static_cast<std::uint8_t>((value >> shift) & 0xffu));
    }
}

inline void hash_string(std::uint64_t* hash, const std::string& value) {
    hash_size(hash, value.size());
    for (unsigned char byte : value) hash_byte(hash, byte);
}
}  // namespace detail

inline Snapshot build(const game_scene_render_queue::Queue& queue) {
    Snapshot snapshot;
    snapshot.source_scene_index = queue.source_scene_index;
    snapshot.guid = queue.guid;
    snapshot.requests.reserve(queue.direct_file_count + queue.sprite_frame_lookup_count);

    for (std::size_t command_index = 0; command_index < queue.sprites.size(); ++command_index) {
        const auto& command = queue.sprites[command_index];
        if (command.resource.kind == game_scene_type0_resource::Kind::kDirectFile &&
            command.direct_asset_relative_path.has_value()) {
            Request request;
            request.kind = Kind::kDirectFile;
            request.sprite_command_index = command_index;
            request.relative_path = *command.direct_asset_relative_path;
            snapshot.requests.push_back(std::move(request));
            continue;
        }
        if (command.resource.kind == game_scene_type0_resource::Kind::kSpriteFrameByName &&
            !command.resource.resource_name.empty()) {
            Request request;
            request.kind = Kind::kSpriteFrameByName;
            request.sprite_command_index = command_index;
            request.frame_name = command.resource.resource_name;
            snapshot.requests.push_back(std::move(request));
        }
    }

    std::uint64_t hash = detail::kFnvOffsetBasis;
    detail::hash_size(&hash, snapshot.source_scene_index);
    detail::hash_string(&hash, snapshot.guid);
    detail::hash_size(&hash, snapshot.requests.size());
    for (const auto& request : snapshot.requests) {
        detail::hash_byte(&hash, static_cast<std::uint8_t>(request.kind));
        detail::hash_size(&hash, request.sprite_command_index);
        detail::hash_string(&hash, request.relative_path);
        detail::hash_string(&hash, request.frame_name);
    }
    // Java receives this as a signed long. Keep the revision positive and
    // reserve zero for "no live render queue" at the JNI boundary.
    hash &= detail::kJavaLongPositiveMask;
    snapshot.revision = hash == 0 ? 1 : hash;
    return snapshot;
}

}  // namespace nevergone::game_scene_direct_asset_requests
