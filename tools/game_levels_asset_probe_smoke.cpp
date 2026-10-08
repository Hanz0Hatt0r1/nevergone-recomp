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
    auto state = probe_file(missing, 16);
    assert(state.configured);
    assert(!state.present);
    assert(!state.loaded);

    const std::string file = root + "/scene.glData";
    write_file(file, std::string("\x01\x02\x03\x04", 4));

    state = probe_file(file, 16);
    assert(state.present);
    assert(state.regular_file);
    assert(state.within_size_limit);
    assert(state.loaded);
    assert(state.file_size == 4);
    assert(state.reader_size == 4);

    state = probe_file(file, 3);
    assert(state.present);
    assert(state.regular_file);
    assert(!state.within_size_limit);
    assert(!state.loaded);

    state = probe_file(root, 16);
    assert(state.present);
    assert(!state.regular_file);
    assert(!state.loaded);

    state = probe_file("", 16);
    assert(!state.configured);

    std::remove(file.c_str());
    rmdir(root.c_str());
    return 0;
}
