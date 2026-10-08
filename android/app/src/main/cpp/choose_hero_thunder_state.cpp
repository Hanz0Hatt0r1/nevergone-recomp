#include "choose_hero_thunder_state.h"

#include <algorithm>
#include <cmath>
#include <limits>

#include "choose_hero_thunder_scheduler.h"

namespace nevergone::choose_hero_thunder_state {
namespace {

constexpr std::uint64_t kPrngMask = (1ULL << 48U) - 1ULL;
constexpr std::uint64_t kPrngMultiplier = 0x5deece66dULL;
constexpr std::uint64_t kPrngIncrement = 0xbULL;
constexpr double kEventEpsilon = 1e-9;

bool due(double event_seconds, double target_seconds) {
    return event_seconds <= target_seconds + kEventEpsilon;
}

}  // namespace

void Machine::reset(std::uint64_t seed_seconds) {
    // POSIX srand48(seed): X0 = (seed << 16) + 0x330e.
    prng_state_ = ((seed_seconds & 0xffffffffULL) << 16U) | 0x330eULL;
    prng_state_ &= kPrngMask;
    initialized_ = false;
    thunder_ = {};
    lightning_ = {};
    ground_light_ = {};
    pending_sounds_.clear();
    rng_draw_count_ = 0;
    thunder_begin_count_ = 0;
    thunder_end_count_ = 0;
    lightning_schedule_count_ = 0;
    ground_light_schedule_count_ = 0;
    sound_callback_count_ = 0;
    last_sound_index_ = -1;
}

std::uint32_t Machine::next_random() {
    prng_state_ = (kPrngMultiplier * prng_state_ + kPrngIncrement) & kPrngMask;
    ++rng_draw_count_;
    return static_cast<std::uint32_t>(prng_state_ >> 17U);
}

void Machine::schedule_thunder(
        std::size_t index,
        double start_seconds,
        float delay_seconds,
        float fade_seconds) {
    if (index >= thunder_.size()) return;
    ThunderAction action;
    action.running = true;
    action.sound_fired = false;
    action.start_seconds = start_seconds;
    action.delay_seconds = std::max(0.0f, delay_seconds);
    action.fade_seconds = std::max(0.0f, fade_seconds);
    thunder_[index] = action;
}

void Machine::schedule_effect(
        EffectAction* action,
        double start_seconds,
        float delay_seconds,
        float fade_seconds) {
    if (action == nullptr) return;
    action->running = true;
    action->start_seconds = start_seconds;
    action->delay_seconds = std::max(0.0f, delay_seconds);
    action->fade_seconds = std::max(0.0f, fade_seconds);
}

void Machine::schedule_initial_thunder(std::size_t index) {
    choose_hero_thunder_scheduler::InitialRandomBatch randoms;
    for (std::uint32_t& discarded : randoms.discarded) discarded = next_random();
    randoms.delay_raw = next_random();
    randoms.fade_raw = next_random();
    const auto plan = choose_hero_thunder_scheduler::initial_plan(randoms);
    schedule_thunder(index, 0.0, plan.delay_seconds, plan.fade_out_seconds);
}

void Machine::start() {
    if (initialized_) return;
    for (std::size_t index = 0; index < thunder_.size(); ++index) {
        schedule_initial_thunder(index);
    }
    initialized_ = true;
}

bool Machine::effect_running_at(const EffectAction& action, double elapsed_seconds) const {
    if (!action.running) return false;
    const double end_seconds = action.start_seconds +
        static_cast<double>(action.delay_seconds) +
        static_cast<double>(action.fade_seconds);
    return elapsed_seconds + kEventEpsilon < end_seconds;
}

float Machine::effect_alpha(const EffectAction& action, double elapsed_seconds) const {
    if (!action.running) return 0.0f;
    const double flash_seconds = action.start_seconds + action.delay_seconds;
    if (elapsed_seconds + kEventEpsilon < flash_seconds) return 0.0f;
    if (action.fade_seconds <= 0.0f) return 0.0f;
    const double progress = (elapsed_seconds - flash_seconds) /
        static_cast<double>(action.fade_seconds);
    if (progress <= 0.0) return 1.0f;
    if (progress >= 1.0) return 0.0f;
    return 1.0f - static_cast<float>(progress);
}

float Machine::thunder_alpha(const ThunderAction& action, double elapsed_seconds) const {
    if (!action.running) return 0.0f;
    const double flash_seconds = action.start_seconds + action.delay_seconds;
    if (elapsed_seconds + kEventEpsilon < flash_seconds) return 0.0f;
    if (action.fade_seconds <= 0.0f) return 0.0f;
    const double progress = (elapsed_seconds - flash_seconds) /
        static_cast<double>(action.fade_seconds);
    if (progress <= 0.0) return 1.0f;
    if (progress >= 1.0) return 0.0f;
    return 1.0f - static_cast<float>(progress);
}

void Machine::process_sound_event(std::size_t thunder_index) {
    if (thunder_index >= thunder_.size()) return;
    ThunderAction& action = thunder_[thunder_index];
    if (!action.running || action.sound_fired) return;
    action.sound_fired = true;
    ++thunder_begin_count_;
    ++sound_callback_count_;
    const int sound_index = choose_hero_thunder_scheduler::thunder_sound_index(next_random());
    last_sound_index_ = sound_index;
    if (sound_index >= 0) pending_sounds_.push_back(sound_index);
}

void Machine::process_end_event(std::size_t thunder_index, double event_seconds) {
    if (thunder_index >= thunder_.size()) return;
    ThunderAction& completed = thunder_[thunder_index];
    if (!completed.running) return;
    completed.running = false;
    ++thunder_end_count_;

    choose_hero_thunder_scheduler::RescheduleRandomBatch randoms;
    randoms.lightning_index_raw_0 = next_random();
    randoms.lightning_index_raw_1 = next_random();
    randoms.lightning_index_raw_2 = next_random();
    randoms.delay_raw = next_random();
    randoms.thunder_fade_raw = next_random();
    const auto plan = choose_hero_thunder_scheduler::reschedule_plan(randoms);

    schedule_thunder(
        thunder_index,
        event_seconds,
        plan.thunder.delay_seconds,
        plan.thunder.fade_out_seconds);

    for (int lightning_index : plan.lightning_indices) {
        if (lightning_index < 0 || lightning_index >= static_cast<int>(lightning_.size())) continue;
        EffectAction& action = lightning_[static_cast<std::size_t>(lightning_index)];
        if (effect_running_at(action, event_seconds)) continue;
        const float fade_seconds =
            choose_hero_thunder_scheduler::lightning_fade_out_seconds(next_random());
        schedule_effect(
            &action,
            event_seconds,
            plan.thunder.delay_seconds,
            fade_seconds);
        ++lightning_schedule_count_;
    }

    // The shipped path advances lrand48 once before checking whether the
    // ground-light node is idle. Preserve that draw even when no action is set.
    (void)next_random();
    if (!effect_running_at(ground_light_, event_seconds)) {
        const float fade_seconds =
            choose_hero_thunder_scheduler::ground_light_fade_out_seconds(next_random());
        schedule_effect(
            &ground_light_,
            event_seconds,
            plan.thunder.delay_seconds,
            fade_seconds);
        ++ground_light_schedule_count_;
    }
}

void Machine::advance(double elapsed_seconds) {
    if (!initialized_) start();
    if (!std::isfinite(elapsed_seconds)) return;
    elapsed_seconds = std::max(0.0, elapsed_seconds);

    while (true) {
        double next_time = std::numeric_limits<double>::infinity();
        std::size_t next_index = thunder_.size();
        bool next_is_sound = false;

        for (std::size_t index = 0; index < thunder_.size(); ++index) {
            const ThunderAction& action = thunder_[index];
            if (!action.running) continue;
            const double sound_time = action.start_seconds + action.delay_seconds;
            const double end_time = sound_time + action.fade_seconds;

            if (!action.sound_fired && due(sound_time, elapsed_seconds)) {
                if (sound_time < next_time - kEventEpsilon ||
                        (std::fabs(sound_time - next_time) <= kEventEpsilon &&
                         (next_index == thunder_.size() || index < next_index ||
                          (index == next_index && !next_is_sound)))) {
                    next_time = sound_time;
                    next_index = index;
                    next_is_sound = true;
                }
            } else if (action.sound_fired && due(end_time, elapsed_seconds)) {
                if (end_time < next_time - kEventEpsilon ||
                        (std::fabs(end_time - next_time) <= kEventEpsilon &&
                         (next_index == thunder_.size() || index < next_index))) {
                    next_time = end_time;
                    next_index = index;
                    next_is_sound = false;
                }
            }
        }

        if (next_index == thunder_.size()) break;
        if (next_is_sound) {
            process_sound_event(next_index);
        } else {
            process_end_event(next_index, next_time);
        }
    }

    for (EffectAction& action : lightning_) {
        if (action.running && !effect_running_at(action, elapsed_seconds)) action.running = false;
    }
    if (ground_light_.running && !effect_running_at(ground_light_, elapsed_seconds)) {
        ground_light_.running = false;
    }
}

Snapshot Machine::snapshot(double elapsed_seconds) const {
    Snapshot result;
    result.initialized = initialized_;
    for (std::size_t index = 0; index < kEffectCount; ++index) {
        result.thunder_alpha[index] = thunder_alpha(thunder_[index], elapsed_seconds);
        result.lightning_alpha[index] = effect_alpha(lightning_[index], elapsed_seconds);
        result.thunder_running[index] = thunder_[index].running;
        result.lightning_running[index] = effect_running_at(lightning_[index], elapsed_seconds);
    }
    result.ground_light_alpha = effect_alpha(ground_light_, elapsed_seconds);
    result.ground_light_running = effect_running_at(ground_light_, elapsed_seconds);
    result.rng_draw_count = rng_draw_count_;
    result.thunder_begin_count = thunder_begin_count_;
    result.thunder_end_count = thunder_end_count_;
    result.lightning_schedule_count = lightning_schedule_count_;
    result.ground_light_schedule_count = ground_light_schedule_count_;
    result.sound_callback_count = sound_callback_count_;
    result.last_sound_index = last_sound_index_;
    return result;
}

int Machine::take_sound_index() {
    if (pending_sounds_.empty()) return -1;
    const int result = pending_sounds_.front();
    pending_sounds_.pop_front();
    return result;
}

}  // namespace nevergone::choose_hero_thunder_state
