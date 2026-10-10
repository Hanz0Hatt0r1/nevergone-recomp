#include <cassert>
#include <cstdint>
#include <vector>

#include "game_scene_direct_asset_store.h"

int main() {
    namespace store = nevergone::game_scene_direct_asset_store;

    const std::uint32_t pixels_a[] = {
        0xff000001u, 0xff000002u,
        0xff000003u, 0xff000004u,
    };
    const std::uint32_t pixels_b[] = {
        0xff000010u, 0xff000011u,
    };

    store::clear();
    auto state = store::snapshot();
    assert(state.active_revision == 0u);
    assert(state.pending_revision == 0u);

    assert(!store::begin(0u, 1u));
    assert(!store::begin(1u, store::kMaxAssetCount + 1u));
    assert(store::begin(11u, 2u));
    state = store::snapshot();
    assert(state.active_revision == 0u);
    assert(state.pending_revision == 11u);
    assert(state.pending_expected_count == 2u);
    assert(state.pending_uploaded_count == 0u);

    assert(!store::upload(12u, 0u, 3u, 2, 2, pixels_a, 4u));
    assert(!store::upload(11u, 2u, 3u, 2, 2, pixels_a, 4u));
    assert(!store::upload(11u, 0u, 3u, 2, 2, pixels_a, 3u));
    assert(store::upload(11u, 0u, 3u, 2, 2, pixels_a, 4u));
    state = store::snapshot();
    assert(state.pending_uploaded_count == 1u);
    assert(state.pending_pixel_count == 4u);
    assert(!store::finish(11u));

    // Replacing a slot must not increase uploaded-count or leak its old pixel count.
    assert(store::upload(11u, 0u, 4u, 2, 1, pixels_b, 2u));
    state = store::snapshot();
    assert(state.pending_uploaded_count == 1u);
    assert(state.pending_pixel_count == 2u);

    assert(store::upload(11u, 1u, 8u, 2, 1, pixels_b, 2u));
    assert(store::finish(11u));
    state = store::snapshot();
    assert(state.active_revision == 11u);
    assert(state.active_asset_count == 2u);
    assert(state.pending_revision == 0u);

    store::Asset copied;
    assert(store::copy_active_asset(0u, &copied));
    assert(copied.request_index == 0u);
    assert(copied.sprite_command_index == 4u);
    assert(copied.width == 2);
    assert(copied.height == 1);
    assert(copied.argb_pixels.size() == 2u);
    assert(copied.argb_pixels[0] == pixels_b[0]);
    assert(store::copy_active_asset(1u, &copied));
    assert(copied.sprite_command_index == 8u);
    assert(!store::copy_active_asset(2u, &copied));

    // A failed/cancelled next revision leaves the completed active set untouched.
    assert(store::begin(12u, 1u));
    assert(store::upload(12u, 0u, 9u, 2, 2, pixels_a, 4u));
    store::cancel(12u);
    state = store::snapshot();
    assert(state.active_revision == 11u);
    assert(state.active_asset_count == 2u);
    assert(state.pending_revision == 0u);

    // Successful replacement publishes atomically.
    assert(store::begin(13u, 1u));
    assert(store::upload(13u, 0u, 15u, 2, 2, pixels_a, 4u));
    assert(store::finish(13u));
    state = store::snapshot();
    assert(state.active_revision == 13u);
    assert(state.active_asset_count == 1u);
    assert(store::copy_active_asset(0u, &copied));
    assert(copied.sprite_command_index == 15u);

    // A live revision with no direct sprites is still a valid completed set.
    assert(store::begin(14u, 0u));
    assert(store::finish(14u));
    state = store::snapshot();
    assert(state.active_revision == 14u);
    assert(state.active_asset_count == 0u);

    store::clear();
    state = store::snapshot();
    assert(state.active_revision == 0u);
    assert(state.pending_revision == 0u);
    return 0;
}
