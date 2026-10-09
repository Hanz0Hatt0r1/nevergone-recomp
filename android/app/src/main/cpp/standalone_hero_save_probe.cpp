#include "standalone_hero_save_probe.h"

#include <sys/stat.h>

#include <array>
#include <string>
#include <utility>

namespace nevergone::standalone_hero_save_probe {
namespace {

constexpr std::array<std::pair<std::uint32_t, const char*>, 2> kSlots{{
    {1u, "DMG_01.sData"},
    {2u, "DMG_02.sData"},
}};

bool regular_file(const std::string& path) {
    struct stat info {};
    return stat(path.c_str(), &info) == 0 && S_ISREG(info.st_mode);
}

}  // namespace

bool is_standalone_hero_save_name(std::string_view name) {
    for (const auto& slot : kSlots) {
        if (name == slot.second) return true;
    }
    return false;
}

std::vector<std::uint32_t> standalone_hero_slots(const std::string& files_dir) {
    std::vector<std::uint32_t> result;
    if (files_dir.empty()) return result;

    for (const auto& slot : kSlots) {
        const std::string path = files_dir + "/" + slot.second;
        if (regular_file(path)) result.push_back(slot.first);
    }
    return result;
}

bool has_standalone_hero_save(const std::string& files_dir) {
    return !standalone_hero_slots(files_dir).empty();
}

}  // namespace nevergone::standalone_hero_save_probe
