#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

namespace nevergone::game_save_data_missions {

// Portable logical record: the native RondomMissionData ID observed at +0x14.
// This does not model the full Cocos2d object layout or its ownership rules.
struct MissionData {
    std::uint32_t id;
};

using MissionArray = std::vector<MissionData*>;

MissionData* get_random_mission_data_with_id(const MissionArray& missions, std::uint32_t id) noexcept;
MissionData* get_random_mission_data_with_index(const MissionArray& missions, std::uint32_t index) noexcept;
void add_random_mission(MissionArray& missions, MissionData* mission);

MissionData* get_equip_random_mission_data_with_id(const MissionArray& missions, std::uint32_t id) noexcept;
MissionData* get_equip_random_mission_data_at_index(const MissionArray& missions, std::uint32_t index) noexcept;
void add_equip_random_mission(MissionArray& missions, MissionData* mission);

}  // namespace nevergone::game_save_data_missions
