#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace nevergone::character_random_name {

struct Row {
    std::string first;
    std::string second;
    std::string third;
};

struct Table {
    std::vector<Row> rows;
};

bool load_csv(const std::string& path, Table* output, std::string* error = nullptr);

// Reconstructs the shipped row selection used by CharacterNameLayer tag 3.
// lrand48() values are converted to single precision, scaled by 2^-31,
// offset by 0.01, absoluted, multiplied by row count, then truncated.
std::size_t recovered_row_index(std::int32_t random_value, std::size_t row_count);

// The shipped callback independently selects column 0 and column 1 from two
// random rows and concatenates them as "%s%s". If the result exceeds the
// CharacterNameLayer 18-byte limit, strcpy(buffer, secondPart) keeps only the
// independently selected column-1 component.
bool generate(
    const Table& table,
    std::int32_t first_random,
    std::int32_t second_random,
    std::string* output,
    std::string* error = nullptr);

// Consumes the current CharacterNameLayer pending randomize request only after
// a name was successfully generated and writes the resulting text back into
// character_name_state. The explicit-random variant is deterministic for host
// smoke tests; fulfill_pending() uses the shipped lrand48 source.
bool fulfill_pending_with_random_values(
    std::int32_t first_random,
    std::int32_t second_random,
    std::string* error = nullptr);
bool fulfill_pending(std::string* error = nullptr);

}  // namespace nevergone::character_random_name
