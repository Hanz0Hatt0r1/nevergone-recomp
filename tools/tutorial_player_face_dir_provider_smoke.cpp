#include <cassert>
#include <iostream>
#include <string>

#include "tutorial_player_face_dir_provider.h"

int main() {
    namespace provider = nevergone::tutorial_player_face_dir_provider;
    namespace control = nevergone::player_control_contract;

    nevergone::player_control_runtime_state::Snapshot state;
    state.generation = 10;

    // Facing is not inferred before the recovered primary controls path has
    // established one of the two bounded horizontal states.
    auto result = provider::resolve(state);
    assert(!result.ready());
    assert(!result.face_dir.reviewed);
    assert(result.face_dir.control_generation == 10);

    state.primary_state = control::PrimaryControlState::kState12;
    state.flip_x = true;
    state.generation = 11;
    result = provider::resolve(state);
    assert(result.ready());
    assert(result.control_consistent);
    assert(result.face_dir.reviewed);
    assert(result.face_dir.control_generation == 11);
    assert(result.face_dir.value == 1);

    state.primary_state = control::PrimaryControlState::kState13;
    state.flip_x = false;
    state.generation = 12;
    result = provider::resolve(state);
    assert(result.ready());
    assert(result.face_dir.value == 0);
    assert(result.face_dir.control_generation == 12);

    // Corrupted/inconsistent snapshots fail closed instead of trusting either
    // the enum or bool independently.
    state.primary_state = control::PrimaryControlState::kState12;
    state.flip_x = false;
    state.generation = 13;
    result = provider::resolve(state);
    assert(!result.ready());
    assert(!result.face_dir.reviewed);
    assert(!result.control_consistent);

    state.primary_state = control::PrimaryControlState::kState13;
    state.flip_x = true;
    state.generation = 14;
    result = provider::resolve(state);
    assert(!result.ready());
    assert(!result.face_dir.reviewed);

    const std::string report = provider::status_report(result);
    assert(report.find("Tutorial PLAYERFACEDIR provider") != std::string::npos);
    assert(report.find("ready: no") != std::string::npos);

    std::cout << "Tutorial PLAYERFACEDIR provider smoke: PASS\n";
    return 0;
}
