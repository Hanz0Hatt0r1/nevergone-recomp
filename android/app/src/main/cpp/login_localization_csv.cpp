#include "login_localization_csv.h"

#include <fstream>
#include <sstream>
#include <utility>
#include <vector>

namespace nevergone::login_localization_csv {
namespace {

bool parse_row(std::string_view line, std::vector<std::string>* fields) {
    if (fields == nullptr) return false;
    fields->clear();
    std::string field;
    bool quoted = false;

    for (std::size_t index = 0; index < line.size(); ++index) {
        const char ch = line[index];
        if (quoted) {
            if (ch == '"') {
                if (index + 1u < line.size() && line[index + 1u] == '"') {
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
        if (ch == '"' && field.empty()) {
            quoted = true;
        } else if (ch == ',') {
            fields->push_back(std::move(field));
            field.clear();
        } else {
            field.push_back(ch);
        }
    }
    if (quoted) return false;
    fields->push_back(std::move(field));
    return true;
}

std::string_view strip_bom(std::string_view value) {
    if (value.size() >= 3u &&
            static_cast<unsigned char>(value[0]) == 0xefu &&
            static_cast<unsigned char>(value[1]) == 0xbbu &&
            static_cast<unsigned char>(value[2]) == 0xbfu) {
        value.remove_prefix(3u);
    }
    return value;
}

}  // namespace

bool resolve_key(
        std::string_view csv,
        std::string_view key,
        std::size_t language_column,
        std::string* output) {
    if (output == nullptr || key.empty()) return false;
    output->clear();

    bool first_row = true;
    std::size_t cursor = 0;
    while (cursor <= csv.size()) {
        const std::size_t newline = csv.find('\n', cursor);
        const std::size_t end = newline == std::string_view::npos ? csv.size() : newline;
        std::string_view line = csv.substr(cursor, end - cursor);
        if (!line.empty() && line.back() == '\r') line.remove_suffix(1u);
        if (first_row) {
            line = strip_bom(line);
            first_row = false;
        }

        if (!line.empty()) {
            std::vector<std::string> fields;
            if (!parse_row(line, &fields)) return false;
            if (fields.size() > language_column && !fields.empty() && fields[0] == key) {
                if (fields[language_column].empty() || fields[language_column] == "NULL") return false;
                *output = fields[language_column];
                return true;
            }
        }

        if (newline == std::string_view::npos) break;
        cursor = newline + 1u;
    }
    return false;
}

bool resolve_key_file(
        const std::string& files_dir,
        std::string_view key,
        std::size_t language_column,
        std::string* output) {
    if (output == nullptr || files_dir.empty()) return false;
    std::ifstream input(files_dir + "/assets/Login/ALL_Loin.csv", std::ios::binary);
    if (!input) return false;
    std::ostringstream bytes;
    bytes << input.rdbuf();
    if (!input.good() && !input.eof()) return false;
    return resolve_key(bytes.str(), key, language_column, output);
}

}  // namespace nevergone::login_localization_csv
