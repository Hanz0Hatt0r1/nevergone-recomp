#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

namespace nevergone::offline_hero_creation {

enum class Status {
    kReady,
    kUnsupportedHero,
    kSlotOccupied,
    kLocalizationMissing,
    kInvalidName,
};

struct Candidate {
    Status status = Status::kUnsupportedHero;
    std::uint32_t slot_id = 0;
    std::string display_name;
};

// Standalone Never Gone uses two fixed hero identities. Slot existence is
// checked at request time, so deleting the corresponding DMG save permits
// creating that hero again. No arbitrary player-entered name is required.
Candidate resolve(
    const std::string& files_dir,
    std::int64_t selected_hero,
    std::size_t language_column);

const char* status_name(Status status);

}  // namespace nevergone::offline_hero_creation
