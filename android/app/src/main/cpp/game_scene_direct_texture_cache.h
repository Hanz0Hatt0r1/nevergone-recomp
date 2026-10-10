#pragma once

#include <cstddef>
#include <cstdint>

namespace nevergone::game_scene_direct_texture_cache {

using TextureHandle = std::uint32_t;

struct Texture {
    std::size_t request_index = 0;
    std::size_t sprite_command_index = 0;
    int width = 0;
    int height = 0;
    TextureHandle handle = 0;
};

struct Snapshot {
    std::uint64_t source_revision = 0;
    std::size_t texture_count = 0;
    std::uint64_t sync_attempt_count = 0;
    bool last_sync_succeeded = false;
};

class Backend {
public:
    virtual ~Backend() = default;
    virtual TextureHandle create_texture(
            int width,
            int height,
            const std::uint32_t* argb_pixels,
            std::size_t pixel_count) = 0;
    virtual void destroy_texture(TextureHandle handle) = 0;
};

// A newly-created EGL context cannot safely delete texture names that belonged
// to the previous context. Drop those stale opaque handles without invoking the
// backend; the next sync will rebuild them from the active native pixel store.
void reset_for_new_context();

// Synchronize the cache to the active atomic direct-asset store revision. A new
// revision is committed only after every texture has been created successfully.
// Failed uploads leave the previous texture revision untouched.
bool sync(Backend& backend);

// Explicitly release the active textures in the current GL context.
void clear(Backend& backend);

Snapshot snapshot();
bool texture_for_sprite_command(std::size_t sprite_command_index, Texture* out);

}  // namespace nevergone::game_scene_direct_texture_cache
