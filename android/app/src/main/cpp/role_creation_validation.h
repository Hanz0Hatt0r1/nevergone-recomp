#pragma once

#include <cstddef>
#include <string>

namespace nevergone::role_creation_validation {

constexpr std::size_t kMaxRoleNameBytes = 21;

enum class Result {
    kValid,
    kEmpty,
    kBlockedByDictionary,
    kTooLong,
};

struct Snapshot {
    Result result = Result::kEmpty;
    std::size_t byte_length = 0;
};

// Mirrors the shipped tag-4 submit checks recovered from ChooseHero::OnCreateback:
// non-empty -> LGG_CheckStringLegal -> strlen <= 21 bytes.
Snapshot validate_role_name(const std::string& value);
const char* result_name(Result result);

}  // namespace nevergone::role_creation_validation
