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
std::string verified_object_header_fixture() {
    return std::string(
            "\x01\x00\x00\x00"  // unresolved signed header field
            "\x01\x00\x00\x00"  // scene_count
            "\x04\x00\x00\x00"  // first scene string length
            "\x7fhero"
            "\x00\x00\x80\x3f"  // 1.0f
            "\x00\x00\x00\x40"  // 2.0f
            "\x01\x00\x00\x00"  // layer_count
            "\x00\x00\x40\x3f"  // layer float
            "\x01\x00\x00\x00"  // object_count
            "\xfd\xff\xff\xff"  // first object int32
            "\x03\x00\x00\x00"  // object string length
            "\xaaobj"
            "\x00\x00\x20\x41"  // 10.0f
            "\x00\x00\xa0\x41"  // 20.0f
            "\x00\x00\xf0\x41"  // 30.0f
            "\x00\x00\x20\x42"  // 40.0f
            "\x00\x00\x48\x42"  // 50.0f
            "\xf9\xff\xff\xff"  // second object int32
            "\x01\x00",           // two bools
            75);
}
}  // namespace

int main() {
    using nevergone::game_levels_asset_probe::probe_file;

    const std::string root = make_temp_dir();
    const std::string missing = root + "/missing.glData";
    auto state = probe_file(missing, 128);
    assert(state.configured && !state.present && !state.loaded);
    assert(!state.first_object_header_readable);

    const std::string file = root + "/scene.glData";
    write_file(file, verified_object_header_fixture());

    state = probe_file(file, 128);
    assert(state.loaded);
    assert(state.scene_prefix_readable && state.scene_prefix_bytes_consumed == 8);
    assert(state.first_scene_header_readable && state.first_scene_header_bytes_consumed == 29);
    assert(state.first_layer_header_readable && state.first_layer_header_bytes_consumed == 37);
    assert(state.first_object_prefix_readable && state.first_object_prefix_bytes_consumed == 45);
    assert(state.first_object_header_readable && state.first_object_header_bytes_consumed == 75);

    const std::string short_file = root + "/short.glData";
    const std::string full = verified_object_header_fixture();
    write_file(short_file, full.substr(0, 74));
    state = probe_file(short_file, 128);
    assert(state.loaded);
    assert(state.first_object_prefix_readable);
    assert(!state.first_object_header_readable);
    assert(state.first_object_header_bytes_consumed == 0);

    const std::string no_object_file = root + "/empty-object-list.glData";
    write_file(no_object_file, std::string(
            "\x01\x00\x00\x00"
            "\x01\x00\x00\x00"
            "\x00\x00\x00\x00"
            "\x00"
            "\x00\x00\x00\x00"
            "\x00\x00\x00\x00"
            "\x01\x00\x00\x00"
            "\x00\x00\x00\x00"
            "\x00\x00\x00\x00",
            33));
    state = probe_file(no_object_file, 128);
    assert(state.first_layer_header_readable);
    assert(!state.first_object_prefix_readable);
    assert(!state.first_object_header_readable);

    state = probe_file(file, 74);
    assert(state.present && !state.within_size_limit && !state.loaded);
    assert(!state.first_object_header_readable);

    state = probe_file(root, 128);
    assert(state.present && !state.regular_file && !state.loaded);
    state = probe_file("", 128);
    assert(!state.configured);

    std::remove(no_object_file.c_str());
    std::remove(short_file.c_str());
    std::remove(file.c_str());
    rmdir(root.c_str());
    return 0;
}
