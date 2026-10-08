#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <deque>

namespace nevergone::choose_hero_thunder_state {

constexpr std::size_t kEffectCount = 6;

struct Snapshot {
    bool initialized = false;
    std::array<float, kEffectCount> thunder_alpha{};
    std::array<float, kEffectCount> lightning_alpha{};
    float ground_light_alpha = 0.0f;
    std::array<bool, kEffectCount> thunder_running{};
    std::array<bool, kEffectCount> lightning_running{};
    bool ground_light_running = false;
    std::uint64_t rng_draw_count = 0;
    std::uint64_t thunder_begin_count = 0;
    std::uint64_t thunder_end_count = 0;
    std::uint64_t lightning_schedule_count = 0;
    std::uint64_t ground_light_schedule_count = 0;
    std::uint64_t sound_callback_count = 0;
    int last_sound_index = -1;
};

class Machine {
public:
    void reset(std::uint64_t seed_seconds);
    void start();
    void advance(double elapsed_seconds);
    Snapshot snapshot(double elapsed_seconds) const;

    // Returns a recovered thunder sound slot 0..6, or -1 when no queued sound
    // is pending. Silent FuncThunderBen callbacks are counted but not queued.
    int take_sound_index();

private:
    struct ThunderAction {
        bool running = false;
        bool sound_fired = false;
        double start_seconds = 0.0;
        float delay_seconds = 0.0f;
        float fade_seconds = 0.0f;
    };

    struct EffectAction {
        bool running = false;
        double start_seconds = 0.0;
        float delay_seconds = 0.0f;
        float fade_seconds = 0.0f;
    };

    std::uint32_t next_random();
    void schedule_initial_thunder(std::size_t index);
    void schedule_thunder(
        std::size_t index,
        double start_seconds,
        float delay_seconds,
        float fade_seconds);
    void schedule_effect(
        EffectAction* action,
        double start_seconds,
        float delay_seconds,
        float fade_seconds);
    bool effect_running_at(const EffectAction& action, double elapsed_seconds) const;
    float effect_alpha(const EffectAction& action, double elapsed_seconds) const;
    float thunder_alpha(const ThunderAction& action, double elapsed_seconds) const;
    void process_sound_event(std::size_t thunder_index);
    void process_end_event(std::size_t thunder_index, double event_seconds);

    std::uint64_t prng_state_ = 0;
    bool initialized_ = false;
    std::array<ThunderAction, kEffectCount> thunder_{};
    std::array<EffectAction, kEffectCount> lightning_{};
    EffectAction ground_light_{};
    std::deque<int> pending_sounds_;
    std::uint64_t rng_draw_count_ = 0;
    std::uint64_t thunder_begin_count_ = 0;
    std::uint64_t thunder_end_count_ = 0;
    std::uint64_t lightning_schedule_count_ = 0;
    std::uint64_t ground_light_schedule_count_ = 0;
    std::uint64_t sound_callback_count_ = 0;
    int last_sound_index_ = -1;
};

}  // namespace nevergone::choose_hero_thunder_state
