#pragma once

#include <cstddef>
#include <vector>

#include "game_scene_direct_asset_requests.h"
#include "game_scene_direct_asset_store.h"
#include "scene_actions_static_direct_geometry.h"

namespace nevergone::scene_actions_static_direct_store_geometry {

enum class Status {
    kReady = 0,
    kStoreUnavailable,
    kRevisionMismatch,
    kAssetCountMismatch,
    kAssetCopyFailed,
    kAssetIdentityMismatch,
    kDirectAssetHidden,
    kCollectorRejected,
};

struct Result {
    Status status = Status::kStoreUnavailable;
    scene_actions_static_direct_geometry::Result collector{};
};

inline Result capture(
        const game_scene_render_queue::Queue& queue,
        float camera_eye_x) {
    Result out;
    const auto requests = game_scene_direct_asset_requests::build(queue);
    const auto store = game_scene_direct_asset_store::snapshot();
    if (store.active_revision == 0u) return out;
    if (store.active_revision != requests.revision) {
        out.status = Status::kRevisionMismatch;
        return out;
    }
    if (store.active_asset_count != requests.requests.size()) {
        out.status = Status::kAssetCountMismatch;
        return out;
    }

    std::vector<scene_actions_static_direct_geometry::AssetSize> assets;
    assets.reserve(requests.requests.size());
    for (std::size_t request_index = 0;
            request_index < requests.requests.size();
            ++request_index) {
        game_scene_direct_asset_store::Asset asset;
        if (!game_scene_direct_asset_store::copy_active_asset(request_index, &asset)) {
            out.status = Status::kAssetCopyFailed;
            return out;
        }
        const auto& request = requests.requests[request_index];
        if (asset.request_index != request_index ||
                asset.sprite_command_index != request.sprite_command_index) {
            out.status = Status::kAssetIdentityMismatch;
            return out;
        }
        if (request.kind != game_scene_direct_asset_requests::Kind::kDirectFile) {
            continue;
        }
        if (asset.hidden) {
            out.status = Status::kDirectAssetHidden;
            return out;
        }
        assets.push_back({asset.sprite_command_index, asset.width, asset.height});
    }

    out.collector = scene_actions_static_direct_geometry::build(
            queue, camera_eye_x, assets);
    if (out.collector.status != scene_actions_static_direct_geometry::Status::kReady) {
        out.status = Status::kCollectorRejected;
        return out;
    }
    out.status = Status::kReady;
    return out;
}

}  // namespace nevergone::scene_actions_static_direct_store_geometry
