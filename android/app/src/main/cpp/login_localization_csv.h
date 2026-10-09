#pragma once

#include <cstddef>
#include <string>
#include <string_view>

namespace nevergone::login_localization_csv {

constexpr std::size_t kKeyColumn = 0;
constexpr std::size_t kSimplifiedChineseColumn = 2;
constexpr std::size_t kTraditionalChineseColumn = 3;
constexpr std::size_t kEnglishColumn = 4;
constexpr std::size_t kFrenchColumn = 6;
constexpr std::size_t kGermanColumn = 7;
constexpr std::size_t kJapaneseColumn = 8;

// Resolves one key from the shipped Login/ALL_Loin.csv using the same
// zero-based language column indices consumed by ManagementLayer::GetPlistString.
bool resolve_key(
    std::string_view csv,
    std::string_view key,
    std::size_t language_column,
    std::string* output);

bool resolve_key_file(
    const std::string& files_dir,
    std::string_view key,
    std::size_t language_column,
    std::string* output);

}  // namespace nevergone::login_localization_csv
