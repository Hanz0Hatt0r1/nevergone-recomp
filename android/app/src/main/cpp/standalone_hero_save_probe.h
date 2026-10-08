#pragma once

#include <string>
#include <string_view>

namespace nevergone::standalone_hero_save_probe {

// Original standalone hero files are formatted as DMG_%02d.sData in the
// Cocos writable directory. %02d is a minimum width, so accept two or more
// decimal digits instead of assuming a fixed slot count.
bool is_standalone_hero_save_name(std::string_view name);

// Returns true when the app-private writable root contains at least one
// regular file matching the recovered standalone hero filename pattern.
bool has_standalone_hero_save(const std::string& files_dir);

}  // namespace nevergone::standalone_hero_save_probe
