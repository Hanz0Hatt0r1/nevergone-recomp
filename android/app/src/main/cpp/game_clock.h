#pragma once

#include <cstdint>
#include <string>

namespace nevergone::game_clock {

constexpr double kFixedStepSeconds = 1.0 / 35.0;

void reset();
int advance();
std::uint64_t tick_count();
std::uint64_t dropped_catchup_count();
std::string status_report();

}  // namespace nevergone::game_clock
