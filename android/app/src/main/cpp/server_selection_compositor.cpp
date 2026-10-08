#include "server_selection_compositor.h"

#include <GLES2/gl2.h>
#include <jni.h>

#include <atomic>
#include <cmath>
#include <cstdint>
#include <mutex>
#include <sstream>

#include "initial_ui_transition.h"
#include "server_selection_layout.h"
#include "server_selection_state.h"
#include "server_selection_view.h"

namespace nevergone::server_selection_compositor {
namespace {

std::atomic<int> g_surface_width{0};
std::atomic<int> g_surface_height{0};
std::atomic<int> g_confirm_pointer{-1};
std::atomic<std::uint64_t> g_draw_count{0};
std::atomic<std::uint64_t> g_owned_touch_count{0};
GLuint g_program = 0;
GLint g_color_uniform = -1;

GLuint compile_shader(GLenum type, const char* source) {
    const GLuint shader = glCreateShader(type);
    if (shader == 0) return 0;
    glShaderSource(shader, 1, &source, nullptr);
    glCompileShader(shader);
    GLint compiled = GL_FALSE;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);
    if (compiled == GL_TRUE) return shader;
    glDeleteShader(shader);
    return 0;
}

GLuint build_program() {
    static constexpr const char* kVertex = R"GLSL(
attribute vec2 aPosition;
void main() {
    gl_Position = vec4(aPosition, 0.0, 1.0);
}
)GLSL";
    static constexpr const char* kFragment = R"GLSL(
precision mediump float;
uniform vec4 uColor;
void main() {
    gl_FragColor = uColor;
}
)GLSL";

    const GLuint vertex = compile_shader(GL_VERTEX_SHADER, kVertex);
    const GLuint fragment = compile_shader(GL_FRAGMENT_SHADER, kFragment);
    if (vertex == 0 || fragment == 0) {
        if (vertex != 0) glDeleteShader(vertex);
        if (fragment != 0) glDeleteShader(fragment);
        return 0;
    }

    const GLuint program = glCreateProgram();
    if (program == 0) {
        glDeleteShader(vertex);
        glDeleteShader(fragment);
        return 0;
    }
    glAttachShader(program, vertex);
    glAttachShader(program, fragment);
    glBindAttribLocation(program, 0, "aPosition");
    glLinkProgram(program);
    glDeleteShader(vertex);
    glDeleteShader(fragment);
    GLint linked = GL_FALSE;
    glGetProgramiv(program, GL_LINK_STATUS, &linked);
    if (linked != GL_TRUE) {
        glDeleteProgram(program);
        return 0;
    }
    return program;
}

bool route_active() {
    return nevergone::initial_ui_transition::snapshot().management_route ==
        nevergone::initial_ui_transition::ManagementRoute::kServerSelection;
}

bool finite_rect(const server_selection_layout::RowRect& rect) {
    return rect.index >= 0 &&
        std::isfinite(rect.left) && std::isfinite(rect.bottom) &&
        std::isfinite(rect.width) && std::isfinite(rect.height) &&
        rect.width > 0.0f && rect.height > 0.0f;
}

void draw_rect(
    const server_selection_layout::RowRect& rect,
    float red,
    float green,
    float blue,
    float alpha) {
    const int surface_width = g_surface_width.load(std::memory_order_relaxed);
    const int surface_height = g_surface_height.load(std::memory_order_relaxed);
    const auto mapping = nevergone::server_selection_view::mapping_for_surface(
        surface_width, surface_height);
    if (g_program == 0 || g_color_uniform < 0 || !mapping.valid || !finite_rect(rect)) return;

    const float x0_px = mapping.offset_x + rect.left * mapping.scale;
    const float x1_px = mapping.offset_x + (rect.left + rect.width) * mapping.scale;
    const float y0_px = mapping.offset_y + rect.bottom * mapping.scale;
    const float y1_px = mapping.offset_y + (rect.bottom + rect.height) * mapping.scale;

    const float width = static_cast<float>(surface_width);
    const float height = static_cast<float>(surface_height);
    const GLfloat x0 = (2.0f * x0_px / width) - 1.0f;
    const GLfloat x1 = (2.0f * x1_px / width) - 1.0f;
    const GLfloat y0 = (2.0f * y0_px / height) - 1.0f;
    const GLfloat y1 = (2.0f * y1_px / height) - 1.0f;
    const GLfloat vertices[] = {
        x0, y1,
        x0, y0,
        x1, y1,
        x1, y0,
    };

    glUseProgram(g_program);
    glUniform4f(g_color_uniform, red, green, blue, alpha);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, vertices);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    glDisableVertexAttribArray(0);
}

server_selection_layout::RowRect panel_rect() {
    server_selection_layout::RowRect rect;
    rect.index = 0;
    rect.left = 24.0f;
    rect.bottom = 24.0f;
    rect.width = server_selection_layout::kDesignWidth - 48.0f;
    rect.height = server_selection_layout::kDesignHeight - 48.0f;
    return rect;
}

void clear_touch(int pointer_id) {
    nevergone::server_selection_state::touch_cancelled(pointer_id);
    int expected = pointer_id;
    g_confirm_pointer.compare_exchange_strong(expected, -1, std::memory_order_relaxed);
}

}  // namespace

void on_surface_created() {
    g_program = 0;
    g_color_uniform = -1;
    const GLuint program = build_program();
    if (program == 0) return;
    const GLint color = glGetUniformLocation(program, "uColor");
    if (color < 0) {
        glDeleteProgram(program);
        return;
    }
    g_program = program;
    g_color_uniform = color;
}

void on_surface_changed(int width, int height) {
    g_surface_width.store(width, std::memory_order_relaxed);
    g_surface_height.store(height, std::memory_order_relaxed);
}

bool active() {
    const auto state = nevergone::server_selection_state::snapshot();
    return route_active() && state.payload_valid && state.server_count > 0;
}

void draw() {
    if (!active() || g_program == 0) return;

    const auto state = nevergone::server_selection_state::snapshot();
    draw_rect(panel_rect(), 0.035f, 0.055f, 0.085f, 0.82f);

    const auto rows = nevergone::server_selection_layout::build_rows(
        state.server_count,
        nevergone::server_selection_view::kFallbackRowWidth,
        nevergone::server_selection_view::kFallbackRowHeight);
    for (const auto& row : rows) {
        if (row.bottom + row.height < 0.0f ||
                row.bottom > server_selection_layout::kDesignHeight) {
            continue;
        }
        const bool selected = row.index == state.selected_index;
        if (selected) {
            draw_rect(row, 0.20f, 0.58f, 0.90f, 0.92f);
        } else {
            draw_rect(row, 0.12f, 0.18f, 0.28f, 0.88f);
        }
    }

    if (state.selected_index >= 0) {
        const auto confirm = nevergone::server_selection_view::confirm_rect();
        if (state.enter_request_pending) {
            draw_rect(confirm, 0.20f, 0.72f, 0.38f, 0.95f);
        } else {
            draw_rect(confirm, 0.82f, 0.48f, 0.16f, 0.95f);
        }
    }

    g_draw_count.fetch_add(1, std::memory_order_relaxed);
}

bool on_touch(int action, int pointer_id, float surface_x, float surface_y) {
    if (!active()) {
        clear_touch(pointer_id);
        return false;
    }

    const int width = g_surface_width.load(std::memory_order_relaxed);
    const int height = g_surface_height.load(std::memory_order_relaxed);
    const auto point = nevergone::server_selection_view::surface_to_design(
        width, height, surface_x, surface_y);
    if (!point.inside_design) {
        if (action == 1 || action == 3 || action == 6) clear_touch(pointer_id);
        return true;
    }

    g_owned_touch_count.fetch_add(1, std::memory_order_relaxed);

    // Android MotionEvent constants: DOWN=0, UP=1, MOVE=2, CANCEL=3,
    // POINTER_DOWN=5, POINTER_UP=6.
    if (action == 0 || action == 5) {
        nevergone::server_selection_state::touch_began(pointer_id, point.y);
        if (nevergone::server_selection_view::confirm_contains(point.x, point.y) &&
                nevergone::server_selection_state::snapshot().selected_index >= 0) {
            g_confirm_pointer.store(pointer_id, std::memory_order_relaxed);
        } else {
            g_confirm_pointer.store(-1, std::memory_order_relaxed);
        }
        return true;
    }

    if (action == 3) {
        clear_touch(pointer_id);
        return true;
    }

    if (action == 1 || action == 6) {
        const int confirm_pointer = g_confirm_pointer.load(std::memory_order_relaxed);
        if (confirm_pointer == pointer_id &&
                nevergone::server_selection_view::confirm_contains(point.x, point.y)) {
            nevergone::server_selection_state::touch_cancelled(pointer_id);
            g_confirm_pointer.store(-1, std::memory_order_relaxed);
            (void)nevergone::server_selection_state::confirm_selection();
            return true;
        }

        const auto state = nevergone::server_selection_state::snapshot();
        const int hit_index = nevergone::server_selection_layout::hit_test(
            state.server_count,
            nevergone::server_selection_view::kFallbackRowWidth,
            nevergone::server_selection_view::kFallbackRowHeight,
            point.x,
            point.y);
        (void)nevergone::server_selection_state::touch_ended(
            pointer_id, point.y, hit_index);
        g_confirm_pointer.store(-1, std::memory_order_relaxed);
        return true;
    }

    return true;
}

std::string status_report() {
    std::ostringstream out;
    out << "server selection compositor: " << (active() ? "active" : "inactive") << "\n";
    out << "server selection surface: "
        << g_surface_width.load(std::memory_order_relaxed) << "x"
        << g_surface_height.load(std::memory_order_relaxed) << "\n";
    out << "server selection visual mode: project-owned fallback"
        << " row=" << nevergone::server_selection_view::kFallbackRowWidth
        << "x" << nevergone::server_selection_view::kFallbackRowHeight << "\n";
    out << "server selection draws: " << g_draw_count.load(std::memory_order_relaxed)
        << " owned-touches=" << g_owned_touch_count.load(std::memory_order_relaxed) << "\n";
    return out.str();
}

}  // namespace nevergone::server_selection_compositor

extern "C" JNIEXPORT void JNICALL
Java_org_nevergone_recomp_GameSurfaceView_nativeOnServerSelectionSurfaceCreated(JNIEnv*, jclass) {
    nevergone::server_selection_compositor::on_surface_created();
}

extern "C" JNIEXPORT void JNICALL
Java_org_nevergone_recomp_GameSurfaceView_nativeOnServerSelectionSurfaceChanged(
    JNIEnv*, jclass, jint width, jint height) {
    nevergone::server_selection_compositor::on_surface_changed(
        static_cast<int>(width), static_cast<int>(height));
}

extern "C" JNIEXPORT void JNICALL
Java_org_nevergone_recomp_GameSurfaceView_nativeDrawServerSelectionLayer(JNIEnv*, jclass) {
    nevergone::server_selection_compositor::draw();
}
