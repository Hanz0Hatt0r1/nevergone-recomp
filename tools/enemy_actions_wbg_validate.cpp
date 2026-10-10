#include <cstddef>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

#include "enemy_actions_wbg_document.h"
#include "hp_data_reader.h"

namespace {

bool read_file(const std::string& path, std::vector<std::uint8_t>* out) {
    if (out == nullptr) return false;

    std::ifstream stream(path, std::ios::binary | std::ios::ate);
    if (!stream) return false;

    const std::streampos end = stream.tellg();
    if (end < 0) return false;

    const auto size = static_cast<std::size_t>(end);
    std::vector<std::uint8_t> bytes(size);
    stream.seekg(0, std::ios::beg);
    if (size != 0u &&
        !stream.read(reinterpret_cast<char*>(bytes.data()), static_cast<std::streamsize>(size))) {
        return false;
    }

    *out = std::move(bytes);
    return true;
}

std::size_t nested_record_count(
        const nevergone::enemy_actions_wbg_prefix::NestedActionFrameBlock& block) {
    std::size_t total = 0;
    for (const auto& group : block.groups) total += group.records.size();
    return total;
}

std::size_t versioned_record_count(
        const nevergone::enemy_actions_wbg_prefix::VersionedActionFrameBlock& block) {
    std::size_t total = 0;
    for (const auto& group : block.groups) total += group.records.size();
    return total;
}

void print_summary(
        const std::string& path,
        std::size_t input_bytes,
        const nevergone::enemy_actions_wbg_document::Document& document) {
    std::cout << "parse_status=ok\n";
    std::cout << "path=" << path << '\n';
    std::cout << "input_bytes=" << input_bytes << '\n';
    std::cout << "bytes_consumed=" << document.bytes_consumed << '\n';
    std::cout << "trailing_bytes=" << document.trailing_bytes << '\n';
    std::cout << "header_word0=" << document.prefix.first_i32 << '\n';
    std::cout << "primary_records=" << document.primary_records.size() << '\n';
    std::cout << "compact_records=" << document.compact_block.records.size() << '\n';
    std::cout << "nested_groups=" << document.nested_block.groups.size() << '\n';
    std::cout << "nested_records=" << nested_record_count(document.nested_block) << '\n';
    std::cout << "versioned_groups=" << document.versioned_block.groups.size() << '\n';
    std::cout << "versioned_records=" << versioned_record_count(document.versioned_block) << '\n';
    std::cout << "fixed_tail_records=" << document.fixed_tail_block.records.size() << '\n';
    std::cout << "combo_tuples=" << document.combo_block.tuples.size() << '\n';
    std::cout << "combo_array_94=" << document.combo_block.array_94_values.size() << '\n';
    std::cout << "combo_array_98=" << document.combo_block.array_98_values.size() << '\n';
    std::cout << "combo_array_9c=" << document.combo_block.array_9c_values.size() << '\n';
    std::cout << "final_table_entries=" << document.final_table.entries.size() << '\n';
}

}  // namespace

int main(int argc, char** argv) {
    bool require_eof = false;
    std::string path;

    for (int i = 1; i < argc; ++i) {
        const std::string arg(argv[i]);
        if (arg == "--require-eof") {
            require_eof = true;
        } else if (path.empty()) {
            path = arg;
        } else {
            std::cerr << "usage: enemy_actions_wbg_validate [--require-eof] <file.wbg>\n";
            return 1;
        }
    }

    if (path.empty()) {
        std::cerr << "usage: enemy_actions_wbg_validate [--require-eof] <file.wbg>\n";
        return 1;
    }

    std::vector<std::uint8_t> bytes;
    if (!read_file(path, &bytes)) {
        std::cerr << "read_status=error\n";
        std::cerr << "path=" << path << '\n';
        return 1;
    }

    nevergone::hp_data::Reader reader(bytes);
    nevergone::enemy_actions_wbg_document::Document document;
    if (!nevergone::enemy_actions_wbg_document::parse(reader, &document)) {
        std::cout << "parse_status=error\n";
        std::cout << "path=" << path << '\n';
        std::cout << "input_bytes=" << bytes.size() << '\n';
        return 2;
    }

    print_summary(path, bytes.size(), document);
    if (require_eof && document.trailing_bytes != 0u) {
        std::cout << "eof_policy=failed\n";
        return 3;
    }

    std::cout << "eof_policy=" << (require_eof ? "passed" : "reported") << '\n';
    return 0;
}
