#include "choose_hero_profile_text.h"

#include <cstdio>
#include <fstream>
#include <sstream>

#include "login_localization_csv.h"
#include "standalone_hero_save_metadata.h"

namespace nevergone::choose_hero_profile_text {
namespace {

bool read_localization_csv(const std::string& files_dir, std::string* output) {
    if (output == nullptr || files_dir.empty()) return false;
    output->clear();
    std::ifstream input(files_dir + "/assets/Login/ALL_Loin.csv", std::ios::binary);
    if (!input) return false;
    std::ostringstream bytes;
    bytes << input.rdbuf();
    if (!input.good() && !input.eof()) return false;
    *output = bytes.str();
    return !output->empty();
}

}  // namespace

bool compose(
        std::uint32_t slot_id,
        std::int32_t level,
        std::int32_t game_hours,
        std::int32_t game_minutes,
        std::string_view localization_csv,
        std::size_t language_column,
        Text* output) {
    if (output == nullptr || (slot_id != 1u && slot_id != 2u)) return false;
    *output = {};

    const std::string name_key = "Hero" + std::to_string(slot_id) + "Name";
    std::string name;
    std::string level_prefix;
    std::string time_prefix;
    if (!login_localization_csv::resolve_key(
            localization_csv, name_key, language_column, &name) ||
            !login_localization_csv::resolve_key(
                localization_csv, "GdUI08", language_column, &level_prefix) ||
            !login_localization_csv::resolve_key(
                localization_csv, "GameUSETime", language_column, &time_prefix)) {
        return false;
    }

    char time_value[64]{};
    const int written = std::snprintf(
        time_value,
        sizeof(time_value),
        "%02d:%02d",
        static_cast<int>(game_hours),
        static_cast<int>(game_minutes));
    if (written <= 0 || static_cast<std::size_t>(written) >= sizeof(time_value)) return false;

    Text composed;
    composed.name = std::move(name);
    composed.level = level_prefix + std::to_string(level);
    composed.play_time = time_prefix + time_value;
    *output = std::move(composed);
    return true;
}

bool load(
        const std::string& files_dir,
        std::uint32_t slot_id,
        std::size_t language_column,
        Text* output) {
    if (output == nullptr) return false;
    *output = {};
    standalone_hero_save_metadata::Metadata metadata;
    if (!standalone_hero_save_metadata::read_file(files_dir, slot_id, &metadata)) return false;
    std::string csv;
    if (!read_localization_csv(files_dir, &csv)) return false;
    return compose(
        metadata.slot_id,
        metadata.level,
        metadata.game_hours,
        metadata.game_minutes,
        csv,
        language_column,
        output);
}

}  // namespace nevergone::choose_hero_profile_text
