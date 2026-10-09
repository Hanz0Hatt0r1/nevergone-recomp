#include <jni.h>

#include "single_select_hero_pointer_router.h"

extern "C" JNIEXPORT void JNICALL
Java_org_nevergone_recomp_FreshRoleCompatOverlay_nativeSetSingleSelectSurfaceSize(
        JNIEnv*, jclass, jint width, jint height) {
    nevergone::single_select_hero_pointer_router::set_surface_size(
        static_cast<int>(width), static_cast<int>(height));
}
