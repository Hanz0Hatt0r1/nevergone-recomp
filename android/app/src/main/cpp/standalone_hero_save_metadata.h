#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace nevergone::standalone_hero_save_metadata {

constexpr std::int32_t kSaveMagic = 447389477;  // 0x1AAA9F25
constexpr std::int32_t kCurrentWriterVersion = 10003;  // 0x2713
constexpr std::int32_t kMinimumAcceptedVersionExclusive = 10001;  // 0x2711
constexpr std::int32_t kMaxDeviceHistory = 5;

struct Metadata {
    std::uint32_t slot_id = 0;
    std::int32_t version_code = 0;
    std::int32_t level = 0;
    std::int32_t game_hours = 0;
    std::int32_t game_minutes = 0;
    std::string name_key;
};

// Parses the shipped space-delimited DMG_%02d.sData prefix far enough to
// recover the fields consumed by ChooseHeroItem. It intentionally stops before
// later save sections whose schemas are not yet proven.
bool parse(std::string_view bytes, std::uint32_t slot_id, Metadata* output);

bool read_file(const std::string& files_dir, std::uint32_t slot_id, Metadata* output);
std::vector<Metadata> load_present(const std::string& files_dir);
std::string status_report(const std::string& files_dir);

}  // namespace nevergone::standalone_hero_save_metadata
