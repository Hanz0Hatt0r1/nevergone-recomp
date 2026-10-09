#include "role_creation_validation.h"

#include "string_validation.h"

namespace nevergone::role_creation_validation {

Snapshot validate_role_name_with_max_bytes(
        const std::string& value,
        std::size_t max_bytes) {
    Snapshot snapshot;
    snapshot.byte_length = value.size();
    if (value.empty()) {
        snapshot.result = Result::kEmpty;
        return snapshot;
    }
    if (!nevergone::string_validation::string_is_legal(value)) {
        snapshot.result = Result::kBlockedByDictionary;
        return snapshot;
    }
    if (value.size() > max_bytes) {
        snapshot.result = Result::kTooLong;
        return snapshot;
    }
    snapshot.result = Result::kValid;
    return snapshot;
}

Snapshot validate_role_name(const std::string& value) {
    return validate_role_name_with_max_bytes(value, kMaxRoleNameBytes);
}

const char* result_name(Result result) {
    switch (result) {
        case Result::kValid: return "valid";
        case Result::kEmpty: return "empty";
        case Result::kBlockedByDictionary: return "blocked-by-dictionary";
        case Result::kTooLong: return "too-long";
    }
    return "unknown";
}

}  // namespace nevergone::role_creation_validation
