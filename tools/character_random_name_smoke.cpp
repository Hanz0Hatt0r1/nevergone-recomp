#include <cassert>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

#include "character_name_state.h"
#include "character_random_name.h"
#include "startup_contract.h"

int main() {
    namespace fs = std::filesystem;
    using nevergone::character_random_name::Row;
    using nevergone::character_random_name::Table;

    const fs::path root = fs::temp_directory_path() / "nevergone_character_random_name";
    fs::remove_all(root);
    fs::create_directories(root / "assets");

    {
        std::ofstream csv(root / "assets" / "RandomName.csv", std::ios::binary);
        csv << "\xef\xbb\xbf" "Alpha,One,NULL\r\n"
            << "Beta,Two,NULL\r\n"
            << "Gamma,Three,NULL\r\n"
            << "Delta,Four,NULL\r\n";
    }

    nevergone::startup::RuntimeConfig config;
    config.files_dir = root.string();
    nevergone::startup::configure(config);

    Table table;
    std::string error;
    assert(nevergone::character_random_name::load_csv(
        (root / "assets" / "RandomName.csv").string(), &table, &error));
    assert(error.empty());
    assert(table.rows.size() == 4);
    assert(table.rows[0].first == "Alpha");
    assert(table.rows[1].second == "Two");
    assert(table.rows[3].third == "NULL");

    // Mirrors int(abs(float(lrand48()) * 2^-31 - 0.01f) * rowCount).
    assert(nevergone::character_random_name::recovered_row_index(0, 4) == 0);
    assert(nevergone::character_random_name::recovered_row_index(0x40000000, 4) == 1);
    assert(nevergone::character_random_name::recovered_row_index(0x7fffffff, 4) == 3);
    assert(nevergone::character_random_name::recovered_row_index(123, 0) == 0);

    std::string generated;
    assert(nevergone::character_random_name::generate(
        table, 0, 0x40000000, &generated, &error));
    assert(generated == "AlphaTwo");

    // When the concatenated UTF-8 byte string exceeds 18 bytes, the shipped
    // callback executes strcpy(buffer, secondPart), retaining column 1 only.
    Table long_table;
    long_table.rows.push_back(Row{"ABCDEFGHIJKLMNO", "123456789", "NULL"});
    assert(nevergone::character_random_name::generate(
        long_table, 0, 0, &generated, &error));
    assert(generated == "123456789");
    assert(generated.size() == 9);

    // Prove that the fallback comes from the independently selected second row,
    // rather than from the first row used for column 0.
    Table split_table;
    split_table.rows.push_back(Row{"ABCDEFGHIJKLMNOPQ", "FirstSecond", "NULL"});
    split_table.rows.push_back(Row{"R", "SecondOnly", "NULL"});
    assert(nevergone::character_random_name::generate(
        split_table, 0, 0x40000000, &generated, &error));
    assert(generated == "SecondOnly");

    // The pending CharacterNameLayer request is consumed only after the CSV
    // loads and a name is generated successfully. Career remains unchanged.
    nevergone::character_name_state::reset();
    nevergone::character_name_state::begin(2);
    auto state = nevergone::character_name_state::snapshot();
    assert(state.active);
    assert(state.career == 2);
    assert(state.randomize_pending);

    assert(nevergone::character_random_name::fulfill_pending_with_random_values(
        0, 0x40000000, &error));
    state = nevergone::character_name_state::snapshot();
    assert(state.active);
    assert(state.career == 2);
    assert(!state.randomize_pending);
    assert(state.role_name == "AlphaTwo");

    assert(!nevergone::character_random_name::fulfill_pending_with_random_values(
        0, 0, &error));
    assert(!error.empty());

    // Malformed input fails without consuming a newly pending randomize action.
    {
        std::ofstream csv(root / "assets" / "RandomName.csv", std::ios::binary | std::ios::trunc);
        csv << "only,two\n";
    }
    assert(nevergone::character_name_state::dispatch_tag(3));
    assert(nevergone::character_name_state::randomize_pending());
    assert(!nevergone::character_random_name::fulfill_pending_with_random_values(
        0, 0, &error));
    assert(nevergone::character_name_state::randomize_pending());

    fs::remove_all(root);
    std::cout << "character random name smoke: ok\n";
    return 0;
}
