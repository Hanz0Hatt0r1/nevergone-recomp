#pragma once

#include <string>

namespace nevergone::string_validation {

// Reconstructed semantic equivalent of Lua_CheckNickName: every UTF-8
// codepoint must be ASCII A-Z/a-z/0-9 or a Han ideograph.
bool nickname_is_valid(const std::string& value);

// Reconstructed observable result of LGG_FilterKeyWords::isLegal() using the
// user-imported newWord.txt dictionary under the configured asset root.
bool string_is_legal(const std::string& value);

}  // namespace nevergone::string_validation
