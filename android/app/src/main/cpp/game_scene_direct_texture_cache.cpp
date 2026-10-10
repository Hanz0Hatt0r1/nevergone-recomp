#include "game_scene_direct_texture_cache.h"

#include <mutex>
#include <unordered_set>
#include <utility>
#include <vector>

#include "game_scene_direct_asset_store.h"

namespace nevergone::game_scene_direct_texture_cache {
namespace {

std::mutex g_mutex;
std::uint64_t g_source_revision = 0;
std::vector<Texture> g_textures;
std::uint64_t g_sync_attempt_count = 0;
bool g_last_sync_succeeded = false;

void destroy_textures(Backend& backend, std::vector<Texture>* textures) {
    if (textures == nullptr) return;
    for (const auto& texture : *textures) {
        if (texture.handle != 0) backend.destroy_texture(texture.handle);
    }
    textures->clear();
}

}  // namespace

void reset_for_new_context() {
    std::lock_guard<std::mutex> lock(g_mutex);
    // Texture names from a lost EGL context are already invalid. Do not pass
    // them to glDeleteTextures through the new context.
    g_source_revision = 0;
    g_textures.clear();
    g_last_sync_succeeded = false;
}

bool sync(Backend& backend) {
    const auto assets = game_scene_direct_asset_store::snapshot();

    std::lock_guard<std::mutex> lock(g_mutex);
    ++g_sync_attempt_count;

    if (assets.active_revision == 0) {
        destroy_textures(backend, &g_textures);
        g_source_revision = 0;
        g_last_sync_succeeded = true;
        return true;
    }

    if (g_source_revision == assets.active_revision &&
        g_textures.size() == assets.active_asset_count) {
        g_last_sync_succeeded = true;
        return true;
    }

    std::vector<Texture> pending;
    pending.reserve(assets.active_asset_count);
    std::unordered_set<std::size_t> seen_sprite_commands;
    seen_sprite_commands.reserve(assets.active_asset_count);

    for (std::size_t request_index = 0;
         request_index < assets.active_asset_count;
         ++request_index) {
        game_scene_direct_asset_store::Asset asset;
        if (!game_scene_direct_asset_store::copy_active_asset(request_index, &asset) ||
            asset.request_index != request_index ||
            asset.width <= 0 || asset.height <= 0 ||
            asset.argb_pixels.empty() ||
            !seen_sprite_commands.insert(asset.sprite_command_index).second) {
            destroy_textures(backend, &pending);
            g_last_sync_succeeded = false;
            return false;
        }

        const TextureHandle handle = backend.create_texture(
                asset.width,
                asset.height,
                asset.argb_pixels.data(),
                asset.argb_pixels.size());
        if (handle == 0) {
            destroy_textures(backend, &pending);
            g_last_sync_succeeded = false;
            return false;
        }

        Texture texture;
        texture.request_index = request_index;
        texture.sprite_command_index = asset.sprite_command_index;
        texture.width = asset.width;
        texture.height = asset.height;
        texture.handle = handle;
        pending.push_back(texture);
    }

    destroy_textures(backend, &g_textures);
    g_textures = std::move(pending);
    g_source_revision = assets.active_revision;
    g_last_sync_succeeded = true;
    return true;
}

void clear(Backend& backend) {
    std::lock_guard<std::mutex> lock(g_mutex);
    destroy_textures(backend, &g_textures);
    g_source_revision = 0;
    g_last_sync_succeeded = true;
}

Snapshot snapshot() {
    std::lock_guard<std::mutex> lock(g_mutex);
    Snapshot state;
    state.source_revision = g_source_revision;
    state.texture_count = g_textures.size();
    state.sync_attempt_count = g_sync_attempt_count;
    state.last_sync_succeeded = g_last_sync_succeeded;
    return state;
}

bool texture_for_sprite_command(std::size_t sprite_command_index, Texture* out) {
    if (out == nullptr) return false;
    std::lock_guard<std::mutex> lock(g_mutex);
    for (const auto& texture : g_textures) {
        if (texture.sprite_command_index == sprite_command_index) {
            *out = texture;
            return true;
        }
    }
    *out = {};
    return false;
}

}  // namespace nevergone::game_scene_direct_texture_cache
