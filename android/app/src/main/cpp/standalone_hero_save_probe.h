#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace nevergone::standalone_hero_save_probe {

// The shipped loader iterates slot ids 1 and 2 only, formatting them as
// DMG_%02d.sData. Other syntactically similar filenames are not part of the
// recovered standalone-hero list.
bool is_standalone_hero_save_name(std::string_view name);

// Returns the existing shipped standalone slots in original loader order.
// Valid values are 1 and 2 only.
std::vector<std::uint32_t> standalone_hero_slots(const std::string& files_dir);

bool has_standalone_hero_save(const std::string& files_dir);

}  // namespace nevergone::standalone_hero_save_probe
