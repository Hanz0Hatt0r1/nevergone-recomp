#include <jni.h>

#include <limits>
#include <mutex>
#include <optional>

#include "game_levels_runtime_state.h"
#include "game_scene_direct_asset_requests.h"

namespace {

using RequestSnapshot = nevergone::game_scene_direct_asset_requests::Snapshot;

std::mutex g_mutex;
std::optional<RequestSnapshot> g_snapshot;

jlong refresh_snapshot() {
    const auto queue = nevergone::game_levels_runtime_state::current_scene_render_queue();
    std::lock_guard<std::mutex> lock(g_mutex);
    if (!queue.has_value()) {
        g_snapshot.reset();
        return 0;
    }
    g_snapshot = nevergone::game_scene_direct_asset_requests::build(*queue);
    return static_cast<jlong>(g_snapshot->revision);
}

}  // namespace

extern "C" JNIEXPORT jlong JNICALL
Java_org_nevergone_recomp_GameSceneDirectAssetRequests_nativeRefresh(
        JNIEnv*, jclass) {
    return refresh_snapshot();
}

extern "C" JNIEXPORT jint JNICALL
Java_org_nevergone_recomp_GameSceneDirectAssetRequests_nativeCount(
        JNIEnv*, jclass) {
    std::lock_guard<std::mutex> lock(g_mutex);
    if (!g_snapshot.has_value() ||
        g_snapshot->requests.size() > static_cast<std::size_t>(std::numeric_limits<jint>::max())) {
        return 0;
    }
    return static_cast<jint>(g_snapshot->requests.size());
}

extern "C" JNIEXPORT jstring JNICALL
Java_org_nevergone_recomp_GameSceneDirectAssetRequests_nativePathAt(
        JNIEnv* env,
        jclass,
        jint request_index) {
    if (env == nullptr || request_index < 0) return nullptr;
    std::lock_guard<std::mutex> lock(g_mutex);
    const std::size_t index = static_cast<std::size_t>(request_index);
    if (!g_snapshot.has_value() || index >= g_snapshot->requests.size()) return nullptr;
    return env->NewStringUTF(g_snapshot->requests[index].relative_path.c_str());
}

extern "C" JNIEXPORT jint JNICALL
Java_org_nevergone_recomp_GameSceneDirectAssetRequests_nativeSpriteCommandIndexAt(
        JNIEnv*,
        jclass,
        jint request_index) {
    if (request_index < 0) return -1;
    std::lock_guard<std::mutex> lock(g_mutex);
    const std::size_t index = static_cast<std::size_t>(request_index);
    if (!g_snapshot.has_value() || index >= g_snapshot->requests.size()) return -1;
    const std::size_t command_index = g_snapshot->requests[index].sprite_command_index;
    if (command_index > static_cast<std::size_t>(std::numeric_limits<jint>::max())) return -1;
    return static_cast<jint>(command_index);
}
