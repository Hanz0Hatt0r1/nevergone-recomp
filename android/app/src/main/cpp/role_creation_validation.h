#pragma once

#include <cstddef>
#include <string>

namespace nevergone::role_creation_validation {

// Legacy ChooseHero::OnCreateback(tag=4) accepts up to 21 UTF-8 bytes.
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

// Shared recovered validation order: non-empty -> LGG_CheckStringLegal ->
// strlen <= max_bytes. Different shipped submit surfaces use different limits.
Snapshot validate_role_name_with_max_bytes(
    const std::string& value,
    std::size_t max_bytes);

// Mirrors the legacy shipped tag-4 submit checks recovered from
// ChooseHero::OnCreateback: non-empty -> LGG_CheckStringLegal -> strlen <= 21.
Snapshot validate_role_name(const std::string& value);
const char* result_name(Result result);

}  // namespace nevergone::role_creation_validation
