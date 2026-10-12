#include <cassert>
#include <filesystem>
#include <fstream>
#include <limits>
#include <string>

#include "game_scene_player_motion_state.h"
#include "minimal_runtime_save.h"
#include "minimal_runtime_save_runtime.h"

namespace motion = nevergone::game_scene_player_motion_state;
namespace save = nevergone::minimal_runtime_save;
namespace runtime = nevergone::minimal_runtime_save_runtime;
namespace spawn_ns = nevergone::game_scene_player_spawn_state;

int main() {
    save::Record source;
    source.scene_index = 4u;
    source.player_index = 1u;
    source.motion_offset_x = 123.5f;
    source.motion_offset_y = -17.25f;
    source.scene_guid = "cp_lesson";

    const auto encoded = save::encode(source);
    assert(!encoded.empty());
    save::Record decoded;
    assert(save::decode(encoded.data(), encoded.size(), &decoded));
    assert(decoded.scene_index == source.scene_index);
    assert(decoded.player_index == source.player_index);
    assert(decoded.motion_offset_x == source.motion_offset_x);
    assert(decoded.motion_offset_y == source.motion_offset_y);
    assert(decoded.scene_guid == source.scene_guid);

    // Exact-length and checksum validation reject truncation and corruption.
    assert(!save::decode(encoded.data(), encoded.size() - 1u, &decoded));
    auto corrupt = encoded;
    corrupt.back() ^= 0x40u;
    assert(!save::decode(corrupt.data(), corrupt.size(), &decoded));

    const auto root = std::filesystem::temp_directory_path() /
        "nevergone-minimal-save-smoke";
    std::error_code ec;
    std::filesystem::remove_all(root, ec);
    std::filesystem::create_directories(root, ec);
    assert(!ec);
    const std::string files_dir = root.string();

    assert(save::write_atomic(files_dir, source));
    save::Record disk;
    assert(save::read(files_dir, &disk));
    assert(disk.scene_guid == source.scene_guid);

    spawn_ns::Snapshot spawn;
    spawn.status = spawn_ns::Status::kReady;
    spawn.generation = 77u;
    spawn.source_scene_index = source.scene_index;
    spawn.scene_guid = source.scene_guid;

    // Simulate a fresh process: normal game flow binds the player first, then
    // the sidecar may restore only because scene GUID/index/player all match.
    runtime::reset_session();
    motion::reset();
    assert(motion::sync_spawn(spawn.generation, 1u, 500u));
    assert(runtime::restore_for_spawn(files_dir, spawn, 1u, 500u));
    auto moved = motion::snapshot();
    assert(moved.offset_x == source.motion_offset_x);
    assert(moved.offset_y == source.motion_offset_y);
    assert(runtime::snapshot().restore_count == 1u);

    // Repeated sync is one-shot and cannot double-apply an offset.
    assert(runtime::restore_for_spawn(files_dir, spawn, 1u, 501u));
    moved = motion::snapshot();
    assert(moved.offset_x == source.motion_offset_x);
    assert(runtime::snapshot().restore_count == 1u);

    // A different reconstructed scene cannot be forced by the save. The record
    // stays pending and becomes applicable if normal flow later reaches it.
    runtime::reset_session();
    motion::reset();
    spawn_ns::Snapshot other = spawn;
    other.generation = 88u;
    other.scene_guid = "another_scene";
    assert(motion::sync_spawn(other.generation, 1u, 600u));
    assert(!runtime::restore_for_spawn(files_dir, other, 1u, 600u));
    assert(runtime::snapshot().pending_record);
    assert(motion::snapshot().offset_x == 0.0f);

    motion::reset();
    spawn.generation = 89u;
    assert(motion::sync_spawn(spawn.generation, 1u, 601u));
    assert(runtime::restore_for_spawn(files_dir, spawn, 1u, 601u));
    assert(motion::snapshot().offset_x == source.motion_offset_x);

    // Snapshot-driven autosave writes the first changed motion immediately and
    // throttles further changed generations until 35 logical ticks have passed.
    runtime::reset_session();
    spawn.generation = 100u;
    motion::Snapshot autosave_motion;
    autosave_motion.ready = true;
    autosave_motion.generation = 10u;
    autosave_motion.spawn_generation = spawn.generation;
    autosave_motion.player_index = 1u;
    autosave_motion.applied_steps = 1u;
    autosave_motion.offset_x = 7.0f;
    autosave_motion.offset_y = 9.0f;
    assert(runtime::autosave_if_due(files_dir, spawn, autosave_motion, 1000u));
    assert(runtime::snapshot().capture_count == 1u);

    autosave_motion.generation = 11u;
    autosave_motion.offset_x = 8.0f;
    assert(runtime::autosave_if_due(files_dir, spawn, autosave_motion, 1001u));
    assert(runtime::snapshot().capture_count == 1u);
    assert(runtime::autosave_if_due(files_dir, spawn, autosave_motion, 1035u));
    assert(runtime::snapshot().capture_count == 2u);
    assert(save::read(files_dir, &disk));
    assert(disk.motion_offset_x == 8.0f);

    // Corrupt on-disk data fails before touching the bound motion cursor.
    {
        std::ofstream stream(save::save_path(files_dir),
                             std::ios::binary | std::ios::trunc);
        stream.write("NGSV\x01", 5);
    }
    runtime::reset_session();
    motion::reset();
    spawn.generation = 90u;
    assert(motion::sync_spawn(spawn.generation, 1u, 700u));
    assert(!runtime::restore_for_spawn(files_dir, spawn, 1u, 700u));
    assert(motion::snapshot().offset_x == 0.0f);
    assert(runtime::snapshot().load_failure_count == 1u);

    // Invalid/unbounded records never serialize.
    source.motion_offset_x = std::numeric_limits<float>::infinity();
    assert(save::encode(source).empty());

    std::filesystem::remove_all(root, ec);
    return 0;
}
