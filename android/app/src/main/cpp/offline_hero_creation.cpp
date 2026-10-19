#include "offline_hero_creation.h"

#include <algorithm>
#include <vector>

#include "login_localization_csv.h"
#include "standalone_hero_save_probe.h"

namespace nevergone::offline_hero_creation {

Candidate resolve(
        const std::string& files_dir,
        std::int64_t selected_hero,
        std::size_t language_column) {
    Candidate result;
    if (selected_hero != 1 && selected_hero != 2) {
        return result;
    }
    result.slot_id = static_cast<std::uint32_t>(selected_hero);

    const std::vector<std::uint32_t> occupied =
        standalone_hero_save_probe::standalone_hero_slots(files_dir);
    if (std::find(occupied.begin(), occupied.end(), result.slot_id) != occupied.end()) {
        result.status = Status::kSlotOccupied;
        return result;
    }

    const std::string key = "Hero" + std::to_string(result.slot_id) + "Name";
    if (!login_localization_csv::resolve_key_file(
            files_dir, key, language_column, &result.display_name)) {
        result.status = Status::kLocalizationMissing;
        return result;
    }
    // CharacterName's retained CreateCharacter boundary accepts at most
    // 18 UTF-8 bytes; reject invalid localized labels rather than truncate.
    if (result.display_name.empty() || result.display_name.size() > 18u) {
        result.status = Status::kInvalidName;
        result.display_name.clear();
        return result;
    }
    result.status = Status::kReady;
    return result;
}

const char* status_name(Status status) {
    switch (status) {
        case Status::kReady: return "ready";
        case Status::kUnsupportedHero: return "unsupported-hero";
        case Status::kSlotOccupied: return "hero-already-created";
        case Status::kLocalizationMissing: return "hero-name-not-found";
        case Status::kInvalidName: return "invalid-hero-name";
    }
    return "unknown";
}

}  // namespace nevergone::offline_hero_creation
