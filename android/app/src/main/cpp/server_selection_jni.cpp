#include <jni.h>

#include "character_name_input.h"
#include "choose_hero_action_control_compositor.h"
#include "choose_hero_role_item_compositor.h"
#include "initial_ui_transition.h"
#include "login_lua_session.h"
#include "management_role_action_control_compositor.h"
#include "server_selection_compositor.h"
#include "server_selection_state.h"
#include "single_select_hero_confirm_input.h"
#include "single_select_hero_input.h"

extern "C" JNIEXPORT jboolean JNICALL
Java_org_nevergone_recomp_GameSurfaceView_nativeOnServerSelectionTouch(
    JNIEnv*,
    jclass,
    jint action,
    jint pointer_id,
    jfloat x,
    jfloat y) {
    if (nevergone::character_name_input::on_touch(
            static_cast<int>(action),
            static_cast<int>(pointer_id),
            static_cast<float>(x),
            static_cast<float>(y))) {
        return JNI_TRUE;
    }

    if (nevergone::single_select_hero_confirm_input::on_touch(
            static_cast<int>(action),
            static_cast<int>(pointer_id),
            static_cast<float>(x),
            static_cast<float>(y))) {
        return JNI_TRUE;
    }

    if (nevergone::single_select_hero_input::on_touch(
            static_cast<int>(action),
            static_cast<int>(pointer_id),
            static_cast<float>(x),
            static_cast<float>(y))) {
        return JNI_TRUE;
    }

    if (nevergone::choose_hero_action_control_compositor::on_touch(
            static_cast<int>(action),
            static_cast<int>(pointer_id),
            static_cast<float>(x),
            static_cast<float>(y))) {
        return JNI_TRUE;
    }

    if (nevergone::management_role_action_control_compositor::on_touch(
            static_cast<int>(action),
            static_cast<int>(pointer_id),
            static_cast<float>(x),
            static_cast<float>(y))) {
        return JNI_TRUE;
    }

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

    if (handled && (action == 1 || action == 6) &&
            nevergone::server_selection_state::snapshot().enter_request_pending) {
        (void)nevergone::login_lua_session::ensure_started();
        if (nevergone::login_lua_session::dispatch_pending_server_request()) {
            (void)nevergone::initial_ui_transition::on_server_enter_dispatch_succeeded();
        }
    }

    return handled ? JNI_TRUE : JNI_FALSE;
}
