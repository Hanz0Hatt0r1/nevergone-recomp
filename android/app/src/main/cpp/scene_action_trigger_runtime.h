#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

#include "scene_action_trigger_plan.h"

namespace nevergone::scene_action_trigger_runtime {

enum class TriggerResult {
    kStarted = 0,
    kAlreadyConsumed,
    kInvalidPlan,
};

struct Command {
    std::size_t slot_index = 0;
    std::uint8_t mode_24e = 0;
    float local_x = 0.0f;
    float local_y = 0.0f;
    std::int32_t child_z_order = 0;
    bool call_play = false;
    bool attach_child = false;
};

struct Snapshot {
    bool trigger_consumed = false;
    std::uint64_t trigger_count = 0;
};

class Runtime {
public:
    TriggerResult trigger(
            const scene_action_trigger_plan::Plan& plan,
            std::array<Command, scene_action_trigger_plan::kSlotCount>* out) {
        if (out == nullptr) return TriggerResult::kInvalidPlan;
        if (snapshot_.trigger_consumed) return TriggerResult::kAlreadyConsumed;

        if (plan.slots[0].slot_index != 0u || plan.slots[0].mode_24e != 0u ||
            plan.slots[1].slot_index != 1u || plan.slots[1].mode_24e != 1u) {
            return TriggerResult::kInvalidPlan;
        }

        std::array<Command, scene_action_trigger_plan::kSlotCount> commands{};
        for (std::size_t i = 0; i < commands.size(); ++i) {
            const auto& slot = plan.slots[i];
            commands[i].slot_index = slot.slot_index;
            commands[i].mode_24e = slot.mode_24e;
            commands[i].local_x = slot.local_x;
            commands[i].local_y = slot.local_y;
            commands[i].child_z_order = slot.child_z_order;
            commands[i].call_play = true;
            commands[i].attach_child = true;
        }

        *out = commands;
        snapshot_.trigger_consumed = true;
        ++snapshot_.trigger_count;
        return TriggerResult::kStarted;
    }

    void reset() {
        snapshot_ = Snapshot{};
    }

    Snapshot snapshot() const {
        return snapshot_;
    }

private:
    Snapshot snapshot_{};
};

}  // namespace nevergone::scene_action_trigger_runtime
