#include <jni.h>

#include "choose_hero_role_item_compositor.h"
#include "login_lua_session.h"
#include "server_selection_compositor.h"
#include "server_selection_state.h"

extern "C" JNIEXPORT jboolean JNICALL
Java_org_nevergone_recomp_GameSurfaceView_nativeOnServerSelectionTouch(
    JNIEnv*,
    jclass,
    jint action,
    jint pointer_id,
    jfloat x,
    jfloat y) {
    // GameSurfaceView already sends every touch through this native router
    // before the generic TapToStart path. Reuse it for the mutually-exclusive
    // offline choose-role route so Java input plumbing remains unchanged.
    if (nevergone::choose_hero_role_item_compositor::on_touch(
            static_cast<int>(action),
            static_cast<int>(pointer_id),
            static_cast<float>(x),
            static_cast<float>(y))) {
        return JNI_TRUE;
    }

    const bool handled = nevergone::server_selection_compositor::on_touch(
        static_cast<int>(action),
        static_cast<int>(pointer_id),
        static_cast<float>(x),
        static_cast<float>(y));

    // UP/POINTER_UP is where the reconstructed confirm control creates its
    // pending EnterRequest. Starting is retryable after a user asset import;
    // failed dispatch leaves the request pending rather than discarding it.
    if (handled && (action == 1 || action == 6) &&
            nevergone::server_selection_state::snapshot().enter_request_pending) {
        (void)nevergone::login_lua_session::ensure_started();
        (void)nevergone::login_lua_session::dispatch_pending_server_request();
    }

    return handled ? JNI_TRUE : JNI_FALSE;
}
