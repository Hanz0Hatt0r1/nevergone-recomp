#include "game_save_data_missions.h"

namespace nevergone::game_save_data_missions {
namespace {

constexpr std::uint32_t kEquipRandomMissionLimit = 3;

MissionData* find_by_id(const MissionArray& missions, std::uint32_t id) noexcept {
    for (std::size_t index = 0; index < missions.size(); ++index) {
        MissionData* mission = missions[index];
        if (mission->id == id) return mission;
    }
    return nullptr;
}

MissionData* get_at_index(const MissionArray& missions, std::uint32_t index) noexcept {
    if (index >= missions.size()) return nullptr;
    return missions[index];
}

void add_if_absent(MissionArray& missions, MissionData* mission) {
    if (find_by_id(missions, mission->id) == nullptr) missions.push_back(mission);
}

}  // namespace

MissionData* get_random_mission_data_with_id(const MissionArray& missions, std::uint32_t id) noexcept {
    return find_by_id(missions, id);
}

MissionData* get_random_mission_data_with_index(const MissionArray& missions, std::uint32_t index) noexcept {
    return get_at_index(missions, index);
}

void add_random_mission(MissionArray& missions, MissionData* mission) {
    add_if_absent(missions, mission);
}

MissionData* get_equip_random_mission_data_with_id(const MissionArray& missions, std::uint32_t id) noexcept {
    return find_by_id(missions, id);
}

MissionData* get_equip_random_mission_data_at_index(const MissionArray& missions, std::uint32_t index) noexcept {
    return get_at_index(missions, index);
}

void add_equip_random_mission(MissionArray& missions, MissionData* mission) {
    if (missions.size() >= kEquipRandomMissionLimit) return;
    add_if_absent(missions, mission);
}

}  // namespace nevergone::game_save_data_missions
