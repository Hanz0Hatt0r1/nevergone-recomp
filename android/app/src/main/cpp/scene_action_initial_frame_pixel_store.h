#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

#include "scene_action_pair_document_store.h"

namespace nevergone::scene_action_initial_frame_pixel_store {

constexpr std::size_t kMaxPixelsPerFrame = 16u * 1024u * 1024u;

struct Asset {
    int variant = 0;
    scene_action_pair_document_store::Slot slot =
            scene_action_pair_document_store::Slot::kBorn;
    std::uint8_t mode_24e = 0;
    int width = 0;
    int height = 0;
    std::vector<std::uint32_t> argb_pixels;
    std::uint64_t generation = 0;
};

void reset();

// Retains one reconstructed, untrimmed ARGB frame only after the corresponding
// TrapActions born/idle pair is ready for the same variant. This store does not
// assign a SceneActions transform or start playback.
bool upload(
        scene_action_pair_document_store::Slot slot,
        int variant,
        std::uint8_t mode_24e,
        int width,
        int height,
        const std::uint32_t* pixels,
        std::size_t pixel_count);

std::optional<Asset> current(scene_action_pair_document_store::Slot slot);
bool ready(scene_action_pair_document_store::Slot slot, int variant);

}  // namespace nevergone::scene_action_initial_frame_pixel_store
