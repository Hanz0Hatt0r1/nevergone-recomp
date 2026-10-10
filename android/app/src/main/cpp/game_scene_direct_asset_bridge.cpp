#include <jni.h>

#include <limits>
#include <optional>

#include "game_levels_runtime_state.h"
#include "game_scene_direct_asset_requests.h"

namespace {

std::optional<nevergone::game_scene_direct_asset_requests::Snapshot> current_requests() {
    const auto queue = nevergone::game_levels_runtime_state::current_scene_render_queue();
    if (!queue.has_value()) return std::nullopt;
    return nevergone::game_scene_direct_asset_requests::build(*queue);
}

}  // namespace

extern "C" JNIEXPORT jlong JNICALL
Java_org_nevergone_recomp_GameSceneDirectAssetRequests_nativeRevision(
        JNIEnv*, jclass) {
    const auto requests = current_requests();
    if (!requests.has_value()) return 0;
    return static_cast<jlong>(requests->revision);
}

extern "C" JNIEXPORT jint JNICALL
Java_org_nevergone_recomp_GameSceneDirectAssetRequests_nativeCount(
        JNIEnv*, jclass) {
    const auto requests = current_requests();
    if (!requests.has_value() ||
        requests->requests.size() > static_cast<std::size_t>(std::numeric_limits<jint>::max())) {
        return 0;
    }
    return static_cast<jint>(requests->requests.size());
}

extern "C" JNIEXPORT jstring JNICALL
Java_org_nevergone_recomp_GameSceneDirectAssetRequests_nativePathAt(
        JNIEnv* env,
        jclass,
        jint request_index) {
    if (env == nullptr || request_index < 0) return nullptr;
    const auto requests = current_requests();
    const std::size_t index = static_cast<std::size_t>(request_index);
    if (!requests.has_value() || index >= requests->requests.size()) return nullptr;
    return env->NewStringUTF(requests->requests[index].relative_path.c_str());
}

extern "C" JNIEXPORT jint JNICALL
Java_org_nevergone_recomp_GameSceneDirectAssetRequests_nativeSpriteCommandIndexAt(
        JNIEnv*,
        jclass,
        jint request_index) {
    if (request_index < 0) return -1;
    const auto requests = current_requests();
    const std::size_t index = static_cast<std::size_t>(request_index);
    if (!requests.has_value() || index >= requests->requests.size()) return -1;
    const std::size_t command_index = requests->requests[index].sprite_command_index;
    if (command_index > static_cast<std::size_t>(std::numeric_limits<jint>::max())) return -1;
    return static_cast<jint>(command_index);
}
