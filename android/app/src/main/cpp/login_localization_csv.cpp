#include "login_localization_csv.h"

#include <fstream>
#include <sstream>
#include <utility>
#include <vector>

namespace nevergone::login_localization_csv {
namespace {

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

    std::vector<std::string> fields;
    std::string field;
    bool quoted = false;
    bool first_row = true;
    for (std::size_t index = 0; index <= csv.size(); ++index) {
        const char ch = index == csv.size() ? '\n' : csv[index];
        if (quoted) {
            if (ch == '"') {
                if (index + 1u < csv.size() && csv[index + 1u] == '"') {
                    field.push_back('"');
                    ++index;
                } else {
                    quoted = false;
                }
            } else if (index == csv.size()) {
                return false;
            } else {
                field.push_back(ch);
            }
            continue;
        }
        if (ch == '"' && field.empty()) {
            quoted = true;
        } else if (ch == ',') {
            fields.push_back(std::move(field));
            field.clear();
        } else if (ch == '\n') {
            if (!field.empty() && field.back() == '\r') field.pop_back();
            fields.push_back(std::move(field));
            field.clear();
            if (first_row && !fields.empty()) {
                fields[0] = std::string(strip_bom(fields[0]));
                first_row = false;
            }
            if (fields.size() > language_column && fields[0] == key) {
                if (fields[language_column].empty() || fields[language_column] == "NULL") return false;
                *output = std::move(fields[language_column]);
                return true;
            }
            fields.clear();
        } else {
            field.push_back(ch);
        }
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
