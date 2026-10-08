#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <string>

#include <sys/stat.h>
#include <unistd.h>

#include "game_levels_asset_probe.h"

namespace {

std::string make_temp_dir() {
    char pattern[] = "/tmp/nevergone-gamelevels-probe-XXXXXX";
    char* result = mkdtemp(pattern);
    assert(result != nullptr);
    return result;
}

void write_file(const std::string& path, const std::string& data) {
    std::ofstream output(path, std::ios::binary);
    assert(output);
    output.write(data.data(), static_cast<std::streamsize>(data.size()));
    assert(output.good());
}

}  // namespace

int main() {
    using nevergone::game_levels_asset_probe::probe_file;

    const std::string root = make_temp_dir();
    const std::string missing = root + "/missing.glData";
    auto state = probe_file(missing, 64);
    assert(state.configured);
    assert(!state.present);
    assert(!state.loaded);
    assert(!state.scene_prefix_readable);
    assert(!state.first_record_header_readable);

    const std::string file = root + "/scene.glData";
    write_file(file, std::string(
            "\x01\x00\x00\x00"  // first i32
            "\x02\x00\x00\x00"  // second u32
            "\x04\x00\x00\x00"  // char payload length
            "\x7f"                  // skipped opaque byte
            "hero"
            "\x00\x00\x80\x3f"  // 1.0f
            "\x00\x00\x00\x40", // 2.0f
            25));

    state = probe_file(file, 64);
    assert(state.present);
    assert(state.regular_file);
    assert(state.within_size_limit);
    assert(state.loaded);
    assert(state.file_size == 25);
    assert(state.reader_size == 25);
    assert(state.scene_prefix_readable);
    assert(state.scene_prefix_bytes_consumed == 12);
    assert(state.first_record_header_readable);
    assert(state.first_record_header_bytes_consumed == 25);

    const std::string short_file = root + "/short.glData";
    write_file(short_file, std::string(
            "\x01\x00\x00\x00"
            "\x02\x00\x00\x00"
            "\x04\x00\x00\x00"
            "\x7fhero"
            "\x00\x00\x80\x3f", // missing second float
            21));
    state = probe_file(short_file, 64);
    assert(state.loaded);
    assert(state.scene_prefix_readable);
    assert(state.scene_prefix_bytes_consumed == 12);
    assert(!state.first_record_header_readable);
    assert(state.first_record_header_bytes_consumed == 0);

    state = probe_file(file, 24);
    assert(state.present);
    assert(state.regular_file);
    assert(!state.within_size_limit);
    assert(!state.loaded);
    assert(!state.scene_prefix_readable);
    assert(!state.first_record_header_readable);

    state = probe_file(root, 64);
    assert(state.present);
    assert(!state.regular_file);
    assert(!state.loaded);

    state = probe_file("", 64);
    assert(!state.configured);

    std::remove(short_file.c_str());
    std::remove(file.c_str());
    rmdir(root.c_str());
    return 0;
}
