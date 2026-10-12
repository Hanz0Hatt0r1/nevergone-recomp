#include <jni.h>

#include <cstdint>

#include "tutorial_loading_compositor.h"

extern "C" JNIEXPORT void JNICALL
Java_org_nevergone_recomp_TutorialLoadingAssetLoader_nativeClearFrames(
        JNIEnv*, jclass) {
    nevergone::tutorial_loading_compositor::clear_frames();
}

extern "C" JNIEXPORT jboolean JNICALL
Java_org_nevergone_recomp_TutorialLoadingAssetLoader_nativeUploadFrame(
        JNIEnv* env,
        jclass,
        jint frame_index,
        jint width,
        jint height,
        jintArray argb_pixels) {
    if (argb_pixels == nullptr) return JNI_FALSE;
    const jsize count = env->GetArrayLength(argb_pixels);
    if (count <= 0) return JNI_FALSE;
    jint* pixels = env->GetIntArrayElements(argb_pixels, nullptr);
    if (pixels == nullptr) return JNI_FALSE;
    const bool ok = nevergone::tutorial_loading_compositor::upload_frame(
        static_cast<int>(frame_index),
        static_cast<int>(width),
        static_cast<int>(height),
        reinterpret_cast<const std::uint32_t*>(pixels),
        static_cast<size_t>(count));
    env->ReleaseIntArrayElements(argb_pixels, pixels, JNI_ABORT);
    return ok ? JNI_TRUE : JNI_FALSE;
}
