#include "character_random_name.h"

#include <cmath>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdlib.h>
#include <utility>

#include "character_name_state.h"
#include "startup_contract.h"

namespace nevergone::character_random_name {
namespace {

void set_error(std::string* error, std::string value) {
    if (error != nullptr) *error = std::move(value);
}

bool parse_csv_row(const std::string& line, Row* output) {
    std::vector<std::string> fields;
    std::string field;
    bool quoted = false;

    for (std::size_t index = 0; index < line.size(); ++index) {
        const char ch = line[index];
        if (quoted) {
            if (ch == '"') {
                if (index + 1 < line.size() && line[index + 1] == '"') {
                    field.push_back('"');
                    ++index;
                } else {
                    quoted = false;
                }
            } else {
                field.push_back(ch);
            }
            continue;
        }

        if (ch == ',' ) {
            fields.push_back(field);
            field.clear();
        } else if (ch == '"' && field.empty()) {
            quoted = true;
        } else {
            field.push_back(ch);
        }
    }
    if (quoted) return false;
    fields.push_back(field);
    if (fields.size() != 3) return false;

    output->first = std::move(fields[0]);
    output->second = std::move(fields[1]);
    output->third = std::move(fields[2]);
    return true;
}

}  // namespace

bool load_csv(const std::string& path, Table* output, std::string* error) {
    if (output == nullptr) {
        set_error(error, "RandomName.csv output table is null");
        return false;
    }

    std::ifstream input(path, std::ios::binary);
    if (!input) {
        set_error(error, "cannot open RandomName.csv: " + path);
        return false;
    }

    Table table;
    std::string line;
    bool first_line = true;
    std::size_t line_number = 0;
    while (std::getline(input, line)) {
        ++line_number;
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (first_line) {
            first_line = false;
            if (line.size() >= 3 &&
                static_cast<unsigned char>(line[0]) == 0xef &&
                static_cast<unsigned char>(line[1]) == 0xbb &&
                static_cast<unsigned char>(line[2]) == 0xbf) {
                line.erase(0, 3);
            }
        }
        if (line.empty()) continue;

        Row row;
        if (!parse_csv_row(line, &row)) {
            std::ostringstream message;
            message << "malformed RandomName.csv row " << line_number;
            set_error(error, message.str());
            return false;
        }
        table.rows.push_back(std::move(row));
    }

    if (table.rows.empty()) {
        set_error(error, "RandomName.csv contains no rows");
        return false;
    }

    *output = std::move(table);
    if (error != nullptr) error->clear();
    return true;
}

std::size_t recovered_row_index(std::int32_t random_value, std::size_t row_count) {
    if (row_count == 0) return 0;

    // The ARMv7 callback converts lrand48() to f32, multiplies by 2^-31,
    // subtracts the 0.01f literal, takes abs(), multiplies by the number of
    // three-column CSV rows, and converts the result back to an integer.
    constexpr float kLrandScale = 4.656612873077392578125e-10f;  // 2^-31
    const float unit = static_cast<float>(random_value) * kLrandScale;
    const float scaled = std::fabs(unit - 0.01f) * static_cast<float>(row_count);
    std::size_t index = static_cast<std::size_t>(scaled);
    if (index >= row_count) index = row_count - 1;
    return index;
}

bool generate(
        const Table& table,
        std::int32_t first_random,
        std::int32_t second_random,
        std::string* output,
        std::string* error) {
    if (output == nullptr) {
        set_error(error, "random-name output is null");
        return false;
    }
    if (table.rows.empty()) {
        set_error(error, "random-name table is empty");
        return false;
    }

    const Row& first_row = table.rows[recovered_row_index(first_random, table.rows.size())];
    const Row& second_row = table.rows[recovered_row_index(second_random, table.rows.size())];

    std::string result = first_row.first + second_row.second;
    if (result.size() > character_name_state::kMaxCharacterNameBytes) {
        // The shipped callback uses strcpy(buffer, firstPart) in this case.
        result = first_row.first;
    }

    *output = std::move(result);
    if (error != nullptr) error->clear();
    return true;
}

bool fulfill_pending_with_random_values(
        std::int32_t first_random,
        std::int32_t second_random,
        std::string* error) {
    const auto before = character_name_state::snapshot();
    if (!before.active || !before.randomize_pending) {
        set_error(error, "no CharacterNameLayer randomize request is pending");
        return false;
    }

    const std::filesystem::path path =
        std::filesystem::path(startup::config().files_dir) / "assets" / "RandomName.csv";
    Table table;
    if (!load_csv(path.string(), &table, error)) return false;

    std::string generated;
    if (!generate(table, first_random, second_random, &generated, error)) return false;

    const auto current = character_name_state::snapshot();
    if (!current.active || !current.randomize_pending ||
        current.generation != before.generation || current.career != before.career) {
        set_error(error, "CharacterNameLayer changed while generating a random name");
        return false;
    }

    std::int64_t career = 0;
    if (!character_name_state::take_randomize_request(&career) || career != before.career) {
        set_error(error, "failed to consume CharacterNameLayer randomize request");
        return false;
    }
    if (!character_name_state::set_role_name(std::move(generated))) {
        set_error(error, "CharacterNameLayer closed before random name was applied");
        return false;
    }

    if (error != nullptr) error->clear();
    return true;
}

bool fulfill_pending(std::string* error) {
    return fulfill_pending_with_random_values(
        static_cast<std::int32_t>(::lrand48()),
        static_cast<std::int32_t>(::lrand48()),
        error);
}

}  // namespace nevergone::character_random_name
