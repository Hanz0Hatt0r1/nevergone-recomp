#pragma once

#include <cstdint>

namespace nevergone::splash_sequence_state {

void reset();
void begin(std::uint64_t start_tick);
bool started();
std::uint64_t generation();
std::uint64_t elapsed_tick(std::uint64_t now_tick);
bool complete(std::uint64_t now_tick);
double single_login_seconds(std::uint64_t now_tick);

}  // namespace nevergone::splash_sequence_state
