#include <cassert>
#include <cstdio>
#include <fstream>
#include <string>

#include <sys/stat.h>
#include <unistd.h>

#include "offline_startup_flow.h"
#include "standalone_hero_save_probe.h"

namespace {

std::string make_temp_dir() {
    char pattern[] = "/tmp/nevergone-offline-flow-XXXXXX";
    char* directory = mkdtemp(pattern);
    assert(directory != nullptr);
    return directory;
}

void write_file(const std::string& path) {
    std::ofstream stream(path, std::ios::binary);
    assert(stream.good());
    stream << "probe";
    stream.close();
}

}  // namespace

int main() {
    using namespace nevergone::offline_startup_flow;
    using nevergone::standalone_hero_save_probe::has_standalone_hero_save;
    using nevergone::standalone_hero_save_probe::is_standalone_hero_save_name;

    assert(is_standalone_hero_save_name("DMG_00.sData"));
    assert(is_standalone_hero_save_name("DMG_12.sData"));
    assert(is_standalone_hero_save_name("DMG_123.sData"));
    assert(!is_standalone_hero_save_name("DMG_1.sData"));
    assert(!is_standalone_hero_save_name("DMG_ab.sData"));
    assert(!is_standalone_hero_save_name("dmg_00.sData"));
    assert(!is_standalone_hero_save_name("DMG_00.sdata"));

    const std::string directory = make_temp_dir();
    assert(!has_standalone_hero_save(directory));
    write_file(directory + "/unrelated.bin");
    assert(!has_standalone_hero_save(directory));

    reset();
    on_auto_login_compat_success(0);
    assert(snapshot().route == Route::kInactive);

    set_standalone_hero_presence(false);
    on_auto_login_compat_success(1);
    auto state = snapshot();
    assert(state.route == Route::kOpeningDialogue);
    assert(state.scene_generation == 1);
    assert(state.standalone_hero_presence_known);
    assert(!state.has_standalone_heroes);
    assert(state.auto_login_success_count == 1);
    assert(state.route_resolution_count == 1);

    // Duplicate success for the same scene is ignored. The TapToStart gate
    // already prevents this, but the compatibility layer keeps the invariant.
    on_auto_login_compat_success(1);
    assert(snapshot().auto_login_success_count == 1);

    reset();
    write_file(directory + "/DMG_00.sData");
    assert(has_standalone_hero_save(directory));
    set_standalone_hero_presence(true);
    on_auto_login_compat_success(2);
    state = snapshot();
    assert(state.route == Route::kChooseRole);
    assert(state.has_standalone_heroes);
    assert(state.auto_login_success_count == 1);
    assert(state.route_resolution_count == 1);

    // The route may wait for a file-system probe and resolve afterward.
    reset();
    on_auto_login_compat_success(3);
    state = snapshot();
    assert(state.route == Route::kRoleProbePending);
    assert(!state.standalone_hero_presence_known);
    set_standalone_hero_presence(false);
    assert(snapshot().route == Route::kOpeningDialogue);

    std::remove((directory + "/DMG_00.sData").c_str());
    std::remove((directory + "/unrelated.bin").c_str());
    rmdir(directory.c_str());
    return 0;
}
