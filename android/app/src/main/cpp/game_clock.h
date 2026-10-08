#pragma once

#include <cstdint>
#include <string>

namespace nevergone::game_clock {

// Original client evidence points to an animation/update interval of 1/35 s.
constexpr double kFixedStepSeconds = 1.0 / 35.0;

// Resets accumulated timing state, for example when the GLES surface is recreated.
void reset();

// Advances the fixed-step clock using monotonic wall time. Returns the number
// of simulation ticks due on this render frame. Catch-up is intentionally
// capped so a long pause cannot trigger an unbounded update spiral.
int advance();

std::uint64_t tick_count();
std::uint64_t dropped_catchup_count();
std::string status_report();

}  // namespace nevergone::game_clock
