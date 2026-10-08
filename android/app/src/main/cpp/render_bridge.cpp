#include "render_bridge.h"

#include <GLES2/gl2.h>
#include <jni.h>

#include <atomic>
#include <cstdint>
#include <mutex>
#include <sstream>

namespace nevergone::render {
namespace {

std::atomic<int> g_width{0};
std::atomic<int> g_height{0};
std::atomic<std::uint64_t> g_frame_count{0};
std::atomic<std::uint64_t> g_touch_count{0};
std::mutex g_touch_mutex;
int g_last_touch_action = -1;
int g_last_touch_pointer = -1;
float g_last_touch_x = 0.0f;
float g_last_touch_y = 0.0f;

void on_surface_created() {
    g_frame_count.store(0, std::memory_order_relaxed);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
}

void on_surface_changed(int width, int height) {
    g_width.store(width, std::memory_order_relaxed);
    g_height.store(height, std::memory_order_relaxed);
    glViewport(0, 0, width, height);
}

void on_draw_frame() {
    glClear(GL_COLOR_BUFFER_BIT);
    g_frame_count.fetch_add(1, std::memory_order_relaxed);
}

void on_touch(int action, int pointer_id, float x, float y) {
    g_touch_count.fetch_add(1, std::memory_order_relaxed);
    std::lock_guard<std::mutex> lock(g_touch_mutex);
    g_last_touch_action = action;
    g_last_touch_pointer = pointer_id;
    g_last_touch_x = x;
    g_last_touch_y = y;
}

}  // namespace

std::string status_report() {
    std::ostringstream out;
    out << "render surface: " << g_width.load(std::memory_order_relaxed)
        << "x" << g_height.load(std::memory_order_relaxed) << "\n";
    out << "render frames: " << g_frame_count.load(std::memory_order_relaxed) << "\n";
    out << "touch events: " << g_touch_count.load(std::memory_order_relaxed) << "\n";
    {
        std::lock_guard<std::mutex> lock(g_touch_mutex);
        if (g_last_touch_action >= 0) {
            out << "last touch: action=" << g_last_touch_action
                << " pointer=" << g_last_touch_pointer
                << " x=" << g_last_touch_x
                << " y=" << g_last_touch_y << "\n";
        }
    }
    return out.str();
}

}  // namespace nevergone::render

extern "C" JNIEXPORT void JNICALL
Java_org_nevergone_recomp_GameSurfaceView_nativeOnSurfaceCreated(JNIEnv*, jclass) {
    nevergone::render::on_surface_created();
}

extern "C" JNIEXPORT void JNICALL
Java_org_nevergone_recomp_GameSurfaceView_nativeOnSurfaceChanged(
    JNIEnv*, jclass, jint width, jint height) {
    nevergone::render::on_surface_changed(static_cast<int>(width), static_cast<int>(height));
}

extern "C" JNIEXPORT void JNICALL
Java_org_nevergone_recomp_GameSurfaceView_nativeOnDrawFrame(JNIEnv*, jclass) {
    nevergone::render::on_draw_frame();
}

extern "C" JNIEXPORT void JNICALL
Java_org_nevergone_recomp_GameSurfaceView_nativeOnTouch(
    JNIEnv*, jclass, jint action, jint pointer_id, jfloat x, jfloat y) {
    nevergone::render::on_touch(
        static_cast<int>(action),
        static_cast<int>(pointer_id),
        static_cast<float>(x),
        static_cast<float>(y));
}
