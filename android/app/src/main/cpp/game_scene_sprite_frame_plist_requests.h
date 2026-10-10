#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "game_levels_global_section.h"

namespace nevergone::game_scene_sprite_frame_plist_requests {

struct Request {
    std::size_t source_global_string_index = 0;
    std::string plist_path;
};

struct Snapshot {
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

// GameLevels::LoadGL_Global() appends every record from the first counted
// string vector to GameLevels + 0x24 in source order. GameScene::loadingTex()
// later iterates that exact array and passes each string unchanged to
// CCSpriteFrameCache::addSpriteFramesWithFile(). Preserve all entries,
// including empty/duplicate paths, because the original does not filter them.
inline Snapshot build(const game_levels_global_section::Section& global) {
    Snapshot snapshot;
    snapshot.requests.reserve(global.strings.size());
    for (std::size_t index = 0; index < global.strings.size(); ++index) {
        snapshot.requests.push_back({index, global.strings[index].value});
    }

    std::uint64_t hash = detail::kFnvOffsetBasis;
    detail::hash_size(&hash, snapshot.requests.size());
    for (const auto& request : snapshot.requests) {
        detail::hash_size(&hash, request.source_global_string_index);
        detail::hash_string(&hash, request.plist_path);
    }
    hash &= detail::kJavaLongPositiveMask;
    snapshot.revision = hash == 0 ? 1 : hash;
    return snapshot;
}

}  // namespace nevergone::game_scene_sprite_frame_plist_requests
