#include <cassert>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

#include "role_creation_validation.h"
#include "startup_contract.h"
#include "string_validation.h"

int main() {
    namespace fs = std::filesystem;
    using nevergone::role_creation_validation::Result;

    const fs::path root = fs::temp_directory_path() / "nevergone_role_creation_validation";
    fs::remove_all(root);
    fs::create_directories(root / "assets");
    {
        std::ofstream dictionary(root / "assets" / "newWord.txt", std::ios::binary);
        // Include a UTF-8 BOM to exercise the same imported-file handling as runtime.
        // Keep the BOM escape separate so the following 'b' is not consumed by the
        // variable-length C++ hexadecimal escape.
        dictionary << "\xef\xbb\xbf" "blocked\nforbidden\r\n";
    }

    nevergone::startup::RuntimeConfig config;
    config.files_dir = root.string();
    nevergone::startup::configure(config);

    assert(!nevergone::string_validation::nickname_is_valid(""));
    assert(nevergone::string_validation::nickname_is_valid("Hero123"));
    assert(nevergone::string_validation::nickname_is_valid("\xe8\x8b\xb1\xe9\x9b\x84"));
    assert(!nevergone::string_validation::nickname_is_valid("Hero-123"));

    auto validation = nevergone::role_creation_validation::validate_role_name("");
    assert(validation.result == Result::kEmpty);
    assert(validation.byte_length == 0);

    validation = nevergone::role_creation_validation::validate_role_name("HeroName");
    assert(validation.result == Result::kValid);
    assert(validation.byte_length == 8);

    // Tag-4 submit checks the keyword filter and byte count, not Lua_CheckNickName.
    validation = nevergone::role_creation_validation::validate_role_name("Hero-Name");
    assert(validation.result == Result::kValid);

    validation = nevergone::role_creation_validation::validate_role_name("xxblockedyy");
    assert(validation.result == Result::kBlockedByDictionary);

    validation = nevergone::role_creation_validation::validate_role_name("forbidden");
    assert(validation.result == Result::kBlockedByDictionary);

    validation = nevergone::role_creation_validation::validate_role_name(
        "123456789012345678901");
    assert(validation.result == Result::kValid);
    assert(validation.byte_length == 21);

    validation = nevergone::role_creation_validation::validate_role_name(
        "1234567890123456789012");
    assert(validation.result == Result::kTooLong);
    assert(validation.byte_length == 22);

    // strlen() in the shipped path counts UTF-8 bytes, not Unicode code points.
    const std::string seven_han =
        "\xe8\x8b\xb1\xe9\x9b\x84\xe8\x8b\xb1\xe9\x9b\x84\xe8\x8b\xb1\xe9\x9b\x84\xe8\x8b\xb1";
    assert(seven_han.size() == 21);
    validation = nevergone::role_creation_validation::validate_role_name(seven_han);
    assert(validation.result == Result::kValid);

    const std::string eight_han = seven_han + "\xe9\x9b\x84";
    assert(eight_han.size() == 24);
    validation = nevergone::role_creation_validation::validate_role_name(eight_han);
    assert(validation.result == Result::kTooLong);

    assert(std::string(nevergone::role_creation_validation::result_name(Result::kValid)) ==
           "valid");

    fs::remove_all(root);
    std::cout << "role creation validation smoke: ok\n";
    return 0;
}
