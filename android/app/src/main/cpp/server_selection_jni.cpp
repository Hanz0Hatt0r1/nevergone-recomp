#include <jni.h>

#include "choose_hero_action_control_compositor.h"
#include "choose_hero_role_item_compositor.h"
#include "login_lua_session.h"
#include "management_role_action_control_compositor.h"
#include "server_selection_compositor.h"
#include "server_selection_state.h"
#include "single_select_hero_pointer_router.h"

extern "C" JNIEXPORT jboolean JNICALL
Java_org_nevergone_recomp_GameSurfaceView_nativeOnServerSelectionTouch(
    JNIEnv*,
    jclass,
    jint action,
    jint pointer_id,
    jfloat x,
    jfloat y) {
    // Java already sends reconstructed scene controls through this first-refusal
    // native bridge before generic TapToStart input. SingleSelectHero is
    // mutually exclusive with the server/management routes, so route its exact
    // recovered rune hit boxes here without adding another MotionEvent path.
    if (nevergone::single_select_hero_pointer_router::on_touch(
            static_cast<int>(action),
            static_cast<int>(pointer_id),
            static_cast<float>(x),
            static_cast<float>(y))) {
        return JNI_TRUE;
    }

    // The recovered ChooseHero fixed buttons live outside the hero-item pane,
    // but the existing role-item route consumes the whole choose-role screen.
    // Give Play/Delete first refusal so their exact type-1 hit boxes can emit
    // the shipped OnCreateback tags 3/8 before item selection handles input.
    if (nevergone::choose_hero_action_control_compositor::on_touch(
            static_cast<int>(action),
            static_cast<int>(pointer_id),
            static_cast<float>(x),
            static_cast<float>(y))) {
        return JNI_TRUE;
    }

    // The online ManagementLayer role route uses the same recovered Play
    // button presentation, but dispatches CharacterID through g_UILogin.
    // Give that control first refusal before its role boards are considered.
    if (nevergone::management_role_action_control_compositor::on_touch(
            static_cast<int>(action),
            static_cast<int>(pointer_id),
            static_cast<float>(x),
            static_cast<float>(y))) {
        return JNI_TRUE;
    }

    // Reuse the existing native router for the mutually-exclusive offline
    // choose-role route so Java input plumbing remains unchanged.
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
