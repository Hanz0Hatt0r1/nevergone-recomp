#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>

#include "tutorial_attack_wave_runtime.h"

namespace nevergone::tutorial_enemy_actor_bindings {

struct Binding {
    bool active = false;
    std::size_t slot = 0;
    std::uint64_t wave_generation = 0;
    unsigned enemy_id = 0;
    int initial_enemy_state = 0;
    int base_health = 0;
    float spawn_x = 0.0f;
    float spawn_y = 0.0f;
};

struct Snapshot {
    std::array<Binding, 3> bindings{};
    std::uint64_t generation = 0;
    std::uint64_t wave_generation = 0;
    std::size_t active_count = 0;
    bool wave_bound = false;
};

class Store {
public:
    void reset();

    // Mirrors only roster slots that have actually materialized. It does not
    // create an EnemyObject, choose an asset, start AI, or invent combat state.
    // A new roster wave atomically invalidates all bindings from the old wave.
    bool sync(const tutorial_attack_wave_runtime::Snapshot& attack);

    bool accepts(std::size_t slot, std::uint64_t expected_wave_generation) const;
    Snapshot snapshot() const;
    std::string status_report() const;

private:
    std::array<Binding, 3> bindings_{};
    std::uint64_t generation_ = 0;
    std::uint64_t wave_generation_ = 0;
    bool wave_bound_ = false;
};

}  // namespace nevergone::tutorial_enemy_actor_bindings
