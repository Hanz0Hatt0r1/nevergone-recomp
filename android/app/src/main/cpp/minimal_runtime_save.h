#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace nevergone::minimal_runtime_save {

constexpr std::uint32_t kFormatVersion = 1u;
constexpr std::size_t kMaxSceneGuidBytes = 256u;
constexpr float kMaxAbsMotionOffset = 1000000.0f;

struct Record {
    std::uint32_t scene_index = 0;
    std::uint32_t player_index = 0;
    float motion_offset_x = 0.0f;
    float motion_offset_y = 0.0f;
    std::string scene_guid;
};

bool valid(const Record& record);
std::vector<std::uint8_t> encode(const Record& record);
bool decode(const std::uint8_t* bytes, std::size_t size, Record* output);

// Project-owned sidecar only. This is intentionally not an implementation of
// the proprietary DMG_*.sData container and does not claim compatibility with
// original save files.
std::string save_path(const std::string& files_dir);
bool write_atomic(const std::string& files_dir, const Record& record);
bool read(const std::string& files_dir, Record* output);

}  // namespace nevergone::minimal_runtime_save
