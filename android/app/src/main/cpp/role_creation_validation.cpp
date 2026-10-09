#include "role_creation_validation.h"

#include "string_validation.h"

namespace nevergone::role_creation_validation {

Snapshot validate_role_name(const std::string& value) {
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
    if (value.size() > kMaxRoleNameBytes) {
        snapshot.result = Result::kTooLong;
        return snapshot;
    }
    snapshot.result = Result::kValid;
    return snapshot;
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
