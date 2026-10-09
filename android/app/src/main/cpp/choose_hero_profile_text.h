#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>

namespace nevergone::choose_hero_profile_text {

struct Text {
    std::string name;
    std::string level;
    std::string play_time;
};

bool compose(
    std::uint32_t slot_id,
    std::int32_t level,
    std::int32_t game_hours,
    std::int32_t game_minutes,
    std::string_view localization_csv,
    std::size_t language_column,
    Text* output);

bool load(
    const std::string& files_dir,
    std::uint32_t slot_id,
    std::size_t language_column,
    Text* output);

}  // namespace nevergone::choose_hero_profile_text
