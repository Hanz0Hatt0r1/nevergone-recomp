#include "single_select_hero_touch_bridge.h"

#include <jni.h>

#include <atomic>
#include <mutex>

#include "fresh_role_compat_state.h"
#include "offline_startup_flow.h"
#include "single_select_hero_rune_layout.h"
#include "single_select_hero_touch_state.h"

namespace nevergone::single_select_hero_touch_bridge {
namespace {

constexpr int kActionDown = 0;
constexpr int kActionUp = 1;
constexpr int kActionMove = 2;
constexpr int kActionCancel = 3;
constexpr int kActionOutside = 4;
constexpr int kActionPointerDown = 5;
constexpr int kActionPointerUp = 6;

std::mutex g_geometry_mutex;
single_select_hero_rune_layout::FrameGeometry
    g_geometry[single_select_hero_rune_layout::kRuneCount]{};
std::atomic<int> g_surface_width{0};
std::atomic<int> g_surface_height{0};

bool route_active() {
    return offline_startup_flow::snapshot().route ==
            offline_startup_flow::Route::kOpeningDialogue &&
        fresh_role_compat_state::snapshot().mode ==
            fresh_role_compat_state::Mode::kCareerSelection;
}

single_select_hero_rune_layout::FrameGeometry geometry_for_tag(int tag) {
    if (!single_select_hero_rune_layout::valid_tag(tag)) return {};
    std::lock_guard<std::mutex> lock(g_geometry_mutex);
    return g_geometry[tag - 1];
}

bool point_inside(int tag, float x, float y) {
    const auto rect = single_select_hero_rune_layout::content_rect_for_surface(
        geometry_for_tag(tag),
        tag,
        g_surface_width.load(),
        g_surface_height.load());
    return single_select_hero_rune_layout::contains(rect, x, y);
}

int hit_tag(float x, float y) {
    for (int tag = 1; tag <= single_select_hero_rune_layout::kRuneCount; ++tag) {
        if (point_inside(tag, x, y)) return tag;
    }
    return 0;
}

}  // namespace

void reset_assets() {
    {
        std::lock_guard<std::mutex> lock(g_geometry_mutex);
        for (auto& geometry : g_geometry) geometry = {};
    }
    single_select_hero_touch_state::reset();
}

bool configure_hitbox(
        int tag,
        int width,
        int height,
        int left,
        int top,
        int source_width,
        int source_height) {
    if (!single_select_hero_rune_layout::valid_tag(tag)) return false;
    single_select_hero_rune_layout::FrameGeometry geometry;
    geometry.width = width;
    geometry.height = height;
    geometry.left = left;
    geometry.top = top;
    geometry.source_width = source_width;
    geometry.source_height = source_height;
    if (!single_select_hero_rune_layout::valid_frame(geometry)) return false;

    std::lock_guard<std::mutex> lock(g_geometry_mutex);
    g_geometry[tag - 1] = geometry;
    return true;
}

void set_surface_size(int width, int height) {
    g_surface_width.store(width > 0 ? width : 0);
    g_surface_height.store(height > 0 ? height : 0);
    if (width <= 0 || height <= 0) single_select_hero_touch_state::reset();
}

bool on_touch(int action, int pointer_id, float x, float y) {
    if (!route_active()) {
        single_select_hero_touch_state::reset();
        return false;
    }

    if (action == kActionDown || action == kActionPointerDown) {
        const int tag = hit_tag(x, y);
        return tag != 0 && single_select_hero_touch_state::begin(pointer_id, tag);
    }

    const auto gesture = single_select_hero_touch_state::snapshot();
    if (gesture.pointer_id != pointer_id || gesture.armed_tag == 0) return false;

    if (action == kActionMove) {
        (void)single_select_hero_touch_state::move(
            pointer_id, point_inside(gesture.armed_tag, x, y));
        return true;
    }

    if (action == kActionUp || action == kActionPointerUp) {
        int activated_tag = 0;
        const bool inside = point_inside(gesture.armed_tag, x, y);
        if (!single_select_hero_touch_state::release(
                pointer_id, inside, &activated_tag)) {
            return false;
        }
        if (activated_tag != 0) {
            // Until the recovered OpenTheDoor/Carousel executor exists, native
            // rune taps use the same explicit transition-collapse compatibility
            // boundary as the temporary Android controls. The sender tag/career
            // itself still passes through unchanged.
            (void)fresh_role_compat_state::select_career(activated_tag);
        }
        return true;
    }

    if (action == kActionCancel || action == kActionOutside) {
        (void)single_select_hero_touch_state::cancel(pointer_id);
        return true;
    }

    return true;
}

}  // namespace nevergone::single_select_hero_touch_bridge

extern "C" JNIEXPORT void JNICALL
Java_org_nevergone_recomp_SingleSelectHeroBaseComposer_nativeClearCareerRuneTouchAssets(
        JNIEnv*, jclass) {
    nevergone::single_select_hero_touch_bridge::reset_assets();
}

extern "C" JNIEXPORT jboolean JNICALL
Java_org_nevergone_recomp_SingleSelectHeroBaseComposer_nativeConfigureCareerRuneHitbox(
        JNIEnv*,
        jclass,
        jint tag,
        jint width,
        jint height,
        jint left,
        jint top,
        jint source_width,
        jint source_height) {
    return nevergone::single_select_hero_touch_bridge::configure_hitbox(
               static_cast<int>(tag),
               static_cast<int>(width),
               static_cast<int>(height),
               static_cast<int>(left),
               static_cast<int>(top),
               static_cast<int>(source_width),
               static_cast<int>(source_height))
        ? JNI_TRUE
        : JNI_FALSE;
}

extern "C" JNIEXPORT void JNICALL
Java_org_nevergone_recomp_FreshRoleCompatOverlay_nativeSetSingleSelectSurfaceSize(
        JNIEnv*, jclass, jint width, jint height) {
    nevergone::single_select_hero_touch_bridge::set_surface_size(
        static_cast<int>(width), static_cast<int>(height));
}
