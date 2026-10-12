#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

namespace nevergone::game_scene_action_lifecycle_contract {

constexpr std::uint32_t kSpecialEventId = 0x12eu;

enum class CommandKind {
    kPlay = 0,
    kApplyReturnedPosition,
    kAttachAtObjectZ,
    kStep,
    kStop,
    kDetach,
};

struct Command {
    CommandKind kind = CommandKind::kPlay;
    std::size_t action_index = 0;
};

struct TriggerOnceResult {
    bool accepted = false;
    bool triggered_once = false;
    std::vector<Command> commands;
};

// GameSceneObject::triggerSceneAction(): when +0x105 is still zero, call
// SceneActionsSystem::play() for every retained action-system entry. For indices
// 0 and 1 only, apply the CCPoint returned by play() as the action-node position.
// Then attach every action node to the GameSceneObject using the serialized
// object Z order. Finally set +0x105 = 1 so subsequent calls are no-ops.
inline TriggerOnceResult trigger_once(bool already_triggered, std::size_t action_count) {
    TriggerOnceResult out;
    out.triggered_once = already_triggered;
    if (already_triggered) return out;

    out.accepted = true;
    out.triggered_once = true;
    out.commands.reserve(action_count * 3u);
    for (std::size_t i = 0; i < action_count; ++i) {
        out.commands.push_back({CommandKind::kPlay, i});
        if (i < 2u) out.commands.push_back({CommandKind::kApplyReturnedPosition, i});
        out.commands.push_back({CommandKind::kAttachAtObjectZ, i});
    }
    return out;
}

struct TickResult {
    std::uint32_t special_event_state = 0;
    std::vector<Command> commands;
};

// GameSceneObject::tick(): every retained SceneActionsSystem is polled. A
// non-done action receives step(dt). A done action is stopped and detached.
// The event-id 0x12e / state 1 branch is preserved as an explicit optional
// special case: after the first completed action is handled, state becomes 2
// and action index 1 is played+attached. Callers must not enable this special
// branch for unrelated klhuo objects without event-id evidence.
inline TickResult tick(
        const std::vector<bool>& is_done,
        std::uint32_t event_id,
        std::uint32_t special_event_state) {
    TickResult out;
    out.special_event_state = special_event_state;
    for (std::size_t i = 0; i < is_done.size(); ++i) {
        if (!is_done[i]) {
            out.commands.push_back({CommandKind::kStep, i});
            continue;
        }
        out.commands.push_back({CommandKind::kStop, i});
        out.commands.push_back({CommandKind::kDetach, i});
        if (event_id == kSpecialEventId && out.special_event_state == 1u &&
                is_done.size() > 1u) {
            out.special_event_state = 2u;
            out.commands.push_back({CommandKind::kPlay, 1u});
            out.commands.push_back({CommandKind::kAttachAtObjectZ, 1u});
        }
    }
    return out;
}

struct SpecialTriggerResult {
    bool accepted = false;
    std::uint32_t special_event_state = 0;
    std::vector<Command> commands;
};

// GameSceneObject::TriggerEvent(): only event id 0x12e, local state 0, and an
// idle action index 0 enter the special sequence. The original sets state 1,
// plays action 0 and attaches it. Position is not rewritten in this entry path.
inline SpecialTriggerResult trigger_special_event(
        std::uint32_t object_event_id,
        std::uint32_t requested_event_id,
        std::uint32_t special_event_state,
        bool action0_is_playing,
        std::size_t action_count) {
    SpecialTriggerResult out;
    out.special_event_state = special_event_state;
    if (object_event_id != requested_event_id || object_event_id != kSpecialEventId ||
            special_event_state != 0u || action_count == 0u || action0_is_playing) {
        return out;
    }
    out.accepted = true;
    out.special_event_state = 1u;
    out.commands.push_back({CommandKind::kPlay, 0u});
    out.commands.push_back({CommandKind::kAttachAtObjectZ, 0u});
    return out;
}

}  // namespace nevergone::game_scene_action_lifecycle_contract
