#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "game_scene_render_queue.h"

namespace nevergone::game_scene_direct_asset_requests {

struct Request {
    std::size_t sprite_command_index = 0;
    std::string relative_path;
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
    snapshot.requests.reserve(queue.direct_file_count);

    for (std::size_t command_index = 0; command_index < queue.sprites.size(); ++command_index) {
        const auto& command = queue.sprites[command_index];
        if (!command.direct_asset_relative_path.has_value()) continue;
        snapshot.requests.push_back({command_index, *command.direct_asset_relative_path});
    }

    std::uint64_t hash = detail::kFnvOffsetBasis;
    detail::hash_size(&hash, snapshot.source_scene_index);
    detail::hash_string(&hash, snapshot.guid);
    detail::hash_size(&hash, snapshot.requests.size());
    for (const auto& request : snapshot.requests) {
        detail::hash_size(&hash, request.sprite_command_index);
        detail::hash_string(&hash, request.relative_path);
    }
    // Revision zero is reserved for "no live render queue" at the JNI boundary.
    snapshot.revision = hash == 0 ? 1 : hash;
    return snapshot;
}

}  // namespace nevergone::game_scene_direct_asset_requests
