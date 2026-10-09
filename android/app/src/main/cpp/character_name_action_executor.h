#pragma once

#include <string>

namespace nevergone::character_name_action_executor {

enum class Outcome {
    kInvalidTag,
    kInactive,
    kValidationRejected,
    kCreateRequestMissing,
    kCreateDispatchFailed,
    kSubmitted,
    kClosed,
    kRandomizeFailed,
    kRandomized,
};

using CreateDispatchFn = bool (*)();
using RandomizeFn = bool (*)(std::string* error);

// Shared executor for the recovered CharacterNameLayer callback tags. The
// injectable form keeps host tests independent from Lua/filesystem runtime
// setup while preserving the production side-effect ordering.
Outcome dispatch_tag_with_callbacks(
    int tag,
    CreateDispatchFn create_dispatch,
    RandomizeFn randomize,
    std::string* error = nullptr);

// Production path:
//   tag 1 -> validate/stage name+career, then dispatch CreateCharacter
//   tag 2 -> close CharacterNameLayer state
//   tag 3 -> fulfill RandomName.csv request
Outcome dispatch_tag(int tag, std::string* error = nullptr);

const char* outcome_name(Outcome outcome);

}  // namespace nevergone::character_name_action_executor
