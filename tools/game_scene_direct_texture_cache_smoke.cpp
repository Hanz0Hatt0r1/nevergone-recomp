#include <cassert>
#include <cstddef>
#include <cstdint>
#include <vector>

#include "game_scene_direct_asset_store.h"
#include "game_scene_direct_texture_cache.h"

namespace {

class FakeBackend final : public nevergone::game_scene_direct_texture_cache::Backend {
public:
    nevergone::game_scene_direct_texture_cache::TextureHandle create_texture(
            int width,
            int height,
            const std::uint32_t* pixels,
            std::size_t pixel_count) override {
        ++create_calls;
        if (fail_on_create_call != 0u && create_calls == fail_on_create_call) return 0;
        assert(width > 0);
        assert(height > 0);
        assert(pixels != nullptr);
        assert(pixel_count == static_cast<std::size_t>(width) *
                static_cast<std::size_t>(height));
        return next_handle++;
    }

    void destroy_texture(
            nevergone::game_scene_direct_texture_cache::TextureHandle handle) override {
        assert(handle != 0);
        destroyed.push_back(handle);
    }

    std::size_t create_calls = 0;
    std::size_t fail_on_create_call = 0;
    nevergone::game_scene_direct_texture_cache::TextureHandle next_handle = 1;
    std::vector<nevergone::game_scene_direct_texture_cache::TextureHandle> destroyed;
};

void publish_revision(
        std::uint64_t revision,
        const std::vector<std::size_t>& sprite_command_indexes) {
    namespace store = nevergone::game_scene_direct_asset_store;
    assert(store::begin(revision, sprite_command_indexes.size()));
    for (std::size_t request = 0; request < sprite_command_indexes.size(); ++request) {
        const std::uint32_t pixels[] = {
                0xff000000u | static_cast<std::uint32_t>(request + 1u),
                0xffffffffu,
        };
        assert(store::upload(
                revision,
                request,
                sprite_command_indexes[request],
                2,
                1,
                pixels,
                2u));
    }
    assert(store::finish(revision));
}

bool contains(
        const std::vector<nevergone::game_scene_direct_texture_cache::TextureHandle>& values,
        nevergone::game_scene_direct_texture_cache::TextureHandle needle) {
    for (const auto value : values) {
        if (value == needle) return true;
    }
    return false;
}

}  // namespace

int main() {
    namespace cache = nevergone::game_scene_direct_texture_cache;
    namespace store = nevergone::game_scene_direct_asset_store;

    FakeBackend backend;
    store::clear();
    cache::reset_for_new_context();

    publish_revision(101u, {7u, 3u});
    assert(cache::sync(backend));
    auto state = cache::snapshot();
    assert(state.source_revision == 101u);
    assert(state.texture_count == 2u);
    assert(state.last_sync_succeeded);
    assert(backend.create_calls == 2u);

    cache::Texture texture7;
    cache::Texture texture3;
    assert(cache::texture_for_sprite_command(7u, &texture7));
    assert(cache::texture_for_sprite_command(3u, &texture3));
    assert(texture7.request_index == 0u);
    assert(texture7.handle == 1u);
    assert(texture3.request_index == 1u);
    assert(texture3.handle == 2u);

    // The same completed store revision must not re-upload textures.
    assert(cache::sync(backend));
    assert(backend.create_calls == 2u);
    assert(backend.destroyed.empty());

    // Fail after creating the first texture of a replacement revision. The
    // partial replacement is destroyed while the previous revision remains.
    publish_revision(102u, {9u, 11u});
    backend.fail_on_create_call = 4u;
    assert(!cache::sync(backend));
    state = cache::snapshot();
    assert(state.source_revision == 101u);
    assert(state.texture_count == 2u);
    assert(!state.last_sync_succeeded);
    assert(contains(backend.destroyed, 3u));
    assert(!contains(backend.destroyed, 1u));
    assert(!contains(backend.destroyed, 2u));
    assert(cache::texture_for_sprite_command(7u, &texture7));
    assert(texture7.handle == 1u);

    // Retry succeeds and only then releases the prior active revision.
    backend.fail_on_create_call = 0u;
    assert(cache::sync(backend));
    state = cache::snapshot();
    assert(state.source_revision == 102u);
    assert(state.texture_count == 2u);
    assert(state.last_sync_succeeded);
    assert(contains(backend.destroyed, 1u));
    assert(contains(backend.destroyed, 2u));
    cache::Texture texture9;
    cache::Texture texture11;
    assert(cache::texture_for_sprite_command(9u, &texture9));
    assert(cache::texture_for_sprite_command(11u, &texture11));
    assert(texture9.handle == 4u);
    assert(texture11.handle == 5u);
    assert(!cache::texture_for_sprite_command(7u, &texture7));

    // No live pixel-store revision releases the current-context textures.
    store::clear();
    assert(cache::sync(backend));
    state = cache::snapshot();
    assert(state.source_revision == 0u);
    assert(state.texture_count == 0u);
    assert(contains(backend.destroyed, 4u));
    assert(contains(backend.destroyed, 5u));

    // Context loss is different: old GL names are invalid already and must be
    // forgotten without routing them through the new context's delete backend.
    publish_revision(103u, {13u});
    assert(cache::sync(backend));
    cache::Texture texture13;
    assert(cache::texture_for_sprite_command(13u, &texture13));
    const auto context_lost_handle = texture13.handle;
    const std::size_t destroys_before_context_reset = backend.destroyed.size();
    cache::reset_for_new_context();
    state = cache::snapshot();
    assert(state.source_revision == 0u);
    assert(state.texture_count == 0u);
    assert(backend.destroyed.size() == destroys_before_context_reset);
    assert(!contains(backend.destroyed, context_lost_handle));

    store::clear();
    return 0;
}
