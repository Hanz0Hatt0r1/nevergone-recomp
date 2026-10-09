#include <cassert>
#include <cstdio>
#include <cstdlib>
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
    using nevergone::standalone_hero_save_probe::standalone_hero_slots;

    assert(is_standalone_hero_save_name("DMG_01.sData"));
    assert(is_standalone_hero_save_name("DMG_02.sData"));
    assert(!is_standalone_hero_save_name("DMG_00.sData"));
    assert(!is_standalone_hero_save_name("DMG_03.sData"));
    assert(!is_standalone_hero_save_name("DMG_12.sData"));
    assert(!is_standalone_hero_save_name("DMG_1.sData"));
    assert(!is_standalone_hero_save_name("dmg_01.sData"));

    const std::string directory = make_temp_dir();
    assert(!has_standalone_hero_save(directory));
    write_file(directory + "/unrelated.bin");
    write_file(directory + "/DMG_00.sData");
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

    on_auto_login_compat_success(1);
    assert(snapshot().auto_login_success_count == 1);

    reset();
    write_file(directory + "/DMG_02.sData");
    auto slots = standalone_hero_slots(directory);
    assert(slots.size() == 1 && slots[0] == 2);
    assert(has_standalone_hero_save(directory));
    set_standalone_hero_presence(true);
    on_auto_login_compat_success(2);
    state = snapshot();
    assert(state.route == Route::kChooseRole);
    assert(state.has_standalone_heroes);
    assert(state.auto_login_success_count == 1);
    assert(state.route_resolution_count == 1);

    write_file(directory + "/DMG_01.sData");
    slots = standalone_hero_slots(directory);
    assert(slots.size() == 2);
    assert(slots[0] == 1 && slots[1] == 2);

    reset();
    on_auto_login_compat_success(3);
    state = snapshot();
    assert(state.route == Route::kRoleProbePending);
    assert(!state.standalone_hero_presence_known);
    set_standalone_hero_presence(false);
    assert(snapshot().route == Route::kOpeningDialogue);

    std::remove((directory + "/DMG_00.sData").c_str());
    std::remove((directory + "/DMG_01.sData").c_str());
    std::remove((directory + "/DMG_02.sData").c_str());
    std::remove((directory + "/unrelated.bin").c_str());
    rmdir(directory.c_str());
    return 0;
}
