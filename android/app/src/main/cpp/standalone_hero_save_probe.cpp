#include "standalone_hero_save_probe.h"

#include <dirent.h>
#include <sys/stat.h>

#include <cctype>
#include <string>

namespace nevergone::standalone_hero_save_probe {

bool is_standalone_hero_save_name(std::string_view name) {
    constexpr std::string_view kPrefix = "DMG_";
    constexpr std::string_view kSuffix = ".sData";
    if (name.size() < kPrefix.size() + 2u + kSuffix.size()) return false;
    if (name.substr(0, kPrefix.size()) != kPrefix) return false;
    if (name.substr(name.size() - kSuffix.size()) != kSuffix) return false;

    const std::string_view digits =
        name.substr(kPrefix.size(), name.size() - kPrefix.size() - kSuffix.size());
    if (digits.size() < 2u) return false;
    for (const char ch : digits) {
        if (!std::isdigit(static_cast<unsigned char>(ch))) return false;
    }
    return true;
}

bool has_standalone_hero_save(const std::string& files_dir) {
    if (files_dir.empty()) return false;

    DIR* directory = opendir(files_dir.c_str());
    if (directory == nullptr) return false;

    bool found = false;
    while (dirent* entry = readdir(directory)) {
        const std::string name(entry->d_name != nullptr ? entry->d_name : "");
        if (!is_standalone_hero_save_name(name)) continue;

        const std::string path = files_dir + "/" + name;
        struct stat info {};
        if (stat(path.c_str(), &info) == 0 && S_ISREG(info.st_mode)) {
            found = true;
            break;
        }
    }

    closedir(directory);
    return found;
}

}  // namespace nevergone::standalone_hero_save_probe
