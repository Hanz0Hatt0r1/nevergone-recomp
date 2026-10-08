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
    assert(state.configured && !state.present && !state.loaded);
    assert(!state.first_object_prefix_readable);

    const std::string file = root + "/scene.glData";
    write_file(file, std::string(
            "\x01\x00\x00\x00"  // unresolved signed header field
            "\x01\x00\x00\x00"  // scene_count
            "\x04\x00\x00\x00"  // first string length
            "\x7fhero"
            "\x00\x00\x80\x3f"  // 1.0f
            "\x00\x00\x00\x40"  // 2.0f
            "\x01\x00\x00\x00"  // layer_count
            "\x00\x00\x40\x3f"  // layer float
            "\x01\x00\x00\x00"  // object_count
            "\xfd\xff\xff\xff"  // first object int32
            "\x09\x00\x00\x00", // first object uint32
            45));

    state = probe_file(file, 64);
    assert(state.loaded);
    assert(state.scene_prefix_readable && state.scene_prefix_bytes_consumed == 8);
    assert(state.first_scene_header_readable && state.first_scene_header_bytes_consumed == 29);
    assert(state.first_layer_header_readable && state.first_layer_header_bytes_consumed == 37);
    assert(state.first_object_prefix_readable && state.first_object_prefix_bytes_consumed == 45);

    const std::string short_file = root + "/short.glData";
    write_file(short_file, std::string(
            "\x01\x00\x00\x00"
            "\x01\x00\x00\x00"
            "\x04\x00\x00\x00"
            "\x7fhero"
            "\x00\x00\x80\x3f"
            "\x00\x00\x00\x40"
            "\x01\x00\x00\x00"
            "\x00\x00\x40\x3f"
            "\x01\x00\x00\x00"
            "\xfd\xff\xff\xff"
            "\x09\x00\x00", // truncated second object-prefix field
            44));
    state = probe_file(short_file, 64);
    assert(state.loaded);
    assert(state.first_layer_header_readable);
    assert(!state.first_object_prefix_readable);
    assert(state.first_object_prefix_bytes_consumed == 0);

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
    state = probe_file(no_object_file, 64);
    assert(state.first_layer_header_readable);
    assert(!state.first_object_prefix_readable);

    state = probe_file(file, 44);
    assert(state.present && !state.within_size_limit && !state.loaded);
    assert(!state.first_object_prefix_readable);

    state = probe_file(root, 64);
    assert(state.present && !state.regular_file && !state.loaded);
    state = probe_file("", 64);
    assert(!state.configured);

    std::remove(no_object_file.c_str());
    std::remove(short_file.c_str());
    std::remove(file.c_str());
    rmdir(root.c_str());
    return 0;
}
