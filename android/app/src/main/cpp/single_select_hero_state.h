#pragma once

#include <cstdint>
#include <string>

namespace nevergone::single_select_hero_state {

constexpr std::int32_t kCareerOne = 1;
constexpr std::int32_t kCareerTwo = 2;

struct ConfirmRequest {
    bool valid = false;
    std::uint64_t generation = 0;
    std::uint64_t sequence = 0;
    std::int32_t career = 0;
};

struct Snapshot {
    bool active = false;
    std::uint64_t generation = 0;
    std::int32_t unavailable_career = 0;
    std::int32_t current_career = 0;
    std::int32_t selected_career = 0;
    bool input_enabled = false;
    bool transition_pending = false;
    std::uint64_t selection_count = 0;
    std::uint64_t confirm_count = 0;
    std::uint64_t consume_count = 0;
    ConfirmRequest pending_confirm;
};

bool valid_career(std::int32_t career);

// Mirrors the recovered SingleSelectHero::initUI() selection boundary. The
// existing saved value compared against career tags 1/2 is represented as
// unavailable_career. Initial selection is career 1 unless that value is 1,
// in which case the shipped scene selects career 2.
void begin(std::int32_t unavailable_career = 0);
void reset();

// OpenTheDoor(false, ...) stores a disabled interaction state while the
// carousel transition runs; OpenTheDoor(true, ...) restores interaction.
void set_input_enabled(bool enabled);
void complete_transition();

// Mirrors menuOpenGC(): selection is accepted only while interaction is
// enabled, only for careers 1/2, and never for the unavailable career. A
// successful change starts another locked transition.
bool select_career(std::int32_t career);

// Mirrors menuConfirm(): confirmation forwards current career unchanged. It
// deliberately does not choose the offline/online downstream branch; consumers
// decide whether the confirmed career creates a local save or CharacterNameLayer.
bool confirm();
bool peek_confirm(ConfirmRequest* output);
bool take_confirm(ConfirmRequest* output);

Snapshot snapshot();
std::string status_report();

}  // namespace nevergone::single_select_hero_state
