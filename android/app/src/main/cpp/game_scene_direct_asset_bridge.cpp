#include <jni.h>

#include <limits>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

#include "game_levels_runtime_state.h"
#include "game_scene_direct_asset_requests.h"
#include "game_scene_player_render_queue.h"

namespace {

using RequestSnapshot = nevergone::game_scene_direct_asset_requests::Snapshot;
using RequestKind = nevergone::game_scene_direct_asset_requests::Kind;

std::mutex g_mutex;
std::optional<RequestSnapshot> g_snapshot;
std::vector<std::string> g_atlas_preload_names;

jlong refresh_snapshot() {
    const auto queue = nevergone::game_levels_runtime_state::current_scene_render_queue();
    const auto atlas_preload_names =
            nevergone::game_levels_runtime_state::current_atlas_preload_names();
    std::lock_guard<std::mutex> lock(g_mutex);
    if (!queue.has_value()) {
        g_snapshot.reset();
        g_atlas_preload_names.clear();
        return 0;
    }
    g_snapshot = nevergone::game_scene_direct_asset_requests::build(
            nevergone::game_scene_player_render_queue::current(*queue));
    g_atlas_preload_names = atlas_preload_names;
    return static_cast<jlong>(g_snapshot->revision);
}

const nevergone::game_scene_direct_asset_requests::Request* request_at(jint request_index) {
    if (request_index < 0 || !g_snapshot.has_value()) return nullptr;
    const std::size_t index = static_cast<std::size_t>(request_index);
    if (index >= g_snapshot->requests.size()) return nullptr;
    return &g_snapshot->requests[index];
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

extern "C" JNIEXPORT jint JNICALL
Java_org_nevergone_recomp_GameSceneDirectAssetRequests_nativeAtlasCount(
        JNIEnv*, jclass) {
    std::lock_guard<std::mutex> lock(g_mutex);
    if (!g_snapshot.has_value() ||
        g_atlas_preload_names.size() > static_cast<std::size_t>(std::numeric_limits<jint>::max())) {
        return 0;
    }
    return static_cast<jint>(g_atlas_preload_names.size());
}

extern "C" JNIEXPORT jstring JNICALL
Java_org_nevergone_recomp_GameSceneDirectAssetRequests_nativeAtlasNameAt(
        JNIEnv* env,
        jclass,
        jint atlas_index) {
    if (env == nullptr || atlas_index < 0) return nullptr;
    std::lock_guard<std::mutex> lock(g_mutex);
    const std::size_t index = static_cast<std::size_t>(atlas_index);
    if (!g_snapshot.has_value() || index >= g_atlas_preload_names.size()) return nullptr;
    const std::string& name = g_atlas_preload_names[index];
    if (name.empty()) return nullptr;
    return env->NewStringUTF(name.c_str());
}

extern "C" JNIEXPORT jint JNICALL
Java_org_nevergone_recomp_GameSceneDirectAssetRequests_nativeKindAt(
        JNIEnv*,
        jclass,
        jint request_index) {
    std::lock_guard<std::mutex> lock(g_mutex);
    const auto* request = request_at(request_index);
    if (request == nullptr) return -1;
    switch (request->kind) {
        case RequestKind::kDirectFile: return 0;
        case RequestKind::kSpriteFrameByName: return 1;
    }
    return -1;
}

extern "C" JNIEXPORT jstring JNICALL
Java_org_nevergone_recomp_GameSceneDirectAssetRequests_nativeValueAt(
        JNIEnv* env,
        jclass,
        jint request_index) {
    if (env == nullptr) return nullptr;
    std::lock_guard<std::mutex> lock(g_mutex);
    const auto* request = request_at(request_index);
    if (request == nullptr) return nullptr;
    const std::string* value = nullptr;
    switch (request->kind) {
        case RequestKind::kDirectFile:
            value = &request->relative_path;
            break;
        case RequestKind::kSpriteFrameByName:
            value = &request->frame_name;
            break;
    }
    if (value == nullptr || value->empty()) return nullptr;
    return env->NewStringUTF(value->c_str());
}

extern "C" JNIEXPORT jstring JNICALL
Java_org_nevergone_recomp_GameSceneDirectAssetRequests_nativePathAt(
        JNIEnv* env,
        jclass,
        jint request_index) {
    if (env == nullptr) return nullptr;
    std::lock_guard<std::mutex> lock(g_mutex);
    const auto* request = request_at(request_index);
    if (request == nullptr || request->kind != RequestKind::kDirectFile ||
        request->relative_path.empty()) {
        return nullptr;
    }
    return env->NewStringUTF(request->relative_path.c_str());
}

extern "C" JNIEXPORT jint JNICALL
Java_org_nevergone_recomp_GameSceneDirectAssetRequests_nativeSpriteCommandIndexAt(
        JNIEnv*,
        jclass,
        jint request_index) {
    std::lock_guard<std::mutex> lock(g_mutex);
    const auto* request = request_at(request_index);
    if (request == nullptr) return -1;
    const std::size_t command_index = request->sprite_command_index;
    if (command_index > static_cast<std::size_t>(std::numeric_limits<jint>::max())) return -1;
    return static_cast<jint>(command_index);
}

extern "C" JNIEXPORT jboolean JNICALL
Java_org_nevergone_recomp_GameSceneDirectAssetRequests_nativeIsPlayerBodyAt(
        JNIEnv*, jclass, jint request_index) {
    std::lock_guard<std::mutex> lock(g_mutex);
    const auto* request = request_at(request_index);
    return request != nullptr && request->player_body ? JNI_TRUE : JNI_FALSE;
}

extern "C" JNIEXPORT jstring JNICALL
Java_org_nevergone_recomp_GameSceneDirectAssetRequests_nativePlayerAtlasPlistAt(
        JNIEnv* env, jclass, jint request_index) {
    if (env == nullptr) return nullptr;
    std::lock_guard<std::mutex> lock(g_mutex);
    const auto* request = request_at(request_index);
    if (request == nullptr || !request->player_body || request->player_atlas_plist.empty()) {
        return nullptr;
    }
    return env->NewStringUTF(request->player_atlas_plist.c_str());
}

extern "C" JNIEXPORT jstring JNICALL
Java_org_nevergone_recomp_GameSceneDirectAssetRequests_nativeActionFrameAtlasPlistAt(
        JNIEnv* env, jclass, jint request_index) {
    if (env == nullptr) return nullptr;
    std::lock_guard<std::mutex> lock(g_mutex);
    const auto* request = request_at(request_index);
    if (request == nullptr || request->action_frame_atlas_plist.empty()) return nullptr;
    return env->NewStringUTF(request->action_frame_atlas_plist.c_str());
}

extern "C" JNIEXPORT jint JNICALL
Java_org_nevergone_recomp_GameSceneDirectAssetRequests_nativePlayerCareerAt(
        JNIEnv*, jclass, jint request_index) {
    std::lock_guard<std::mutex> lock(g_mutex);
    const auto* request = request_at(request_index);
    return request != nullptr && request->player_body ? request->player_career : 0;
}

extern "C" JNIEXPORT jint JNICALL
Java_org_nevergone_recomp_GameSceneDirectAssetRequests_nativePlayerClothesIdAt(
        JNIEnv*, jclass, jint request_index) {
    std::lock_guard<std::mutex> lock(g_mutex);
    const auto* request = request_at(request_index);
    return request != nullptr && request->player_body ? request->player_clothes_id : -1;
}
