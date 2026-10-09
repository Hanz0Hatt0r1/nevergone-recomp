#include "server_selection_compositor.h"

#include <GLES2/gl2.h>
#include <jni.h>

#include <atomic>
#include <cmath>
#include <cstdint>
#include <sstream>
#include <vector>

#include "initial_ui_transition.h"
#include "server_selection_assets.h"
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
GLuint g_color_program = 0;
GLint g_color_uniform = -1;
GLuint g_texture_program = 0;
GLint g_texture_sampler = -1;
GLuint g_row_texture = 0;
std::uint64_t g_row_texture_generation = 0;

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

GLuint build_color_program() {
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

GLuint build_texture_program() {
    static constexpr const char* kVertex = R"GLSL(
attribute vec2 aPosition;
attribute vec2 aTexCoord;
varying vec2 vTexCoord;
void main() {
    vTexCoord = aTexCoord;
    gl_Position = vec4(aPosition, 0.0, 1.0);
}
)GLSL";
    static constexpr const char* kFragment = R"GLSL(
precision mediump float;
varying vec2 vTexCoord;
uniform sampler2D uTexture;
void main() {
    gl_FragColor = texture2D(uTexture, vTexCoord);
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
    glBindAttribLocation(program, 1, "aTexCoord");
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

bool row_dimensions(float* width, float* height) {
    if (width == nullptr || height == nullptr) return false;
    int asset_width = 0;
    int asset_height = 0;
    if (nevergone::server_selection_assets::dimensions(
            nevergone::server_selection_assets::kBorder2,
            &asset_width,
            &asset_height)) {
        *width = static_cast<float>(asset_width);
        *height = static_cast<float>(asset_height);
        return true;
    }
    *width = nevergone::server_selection_view::kFallbackRowWidth;
    *height = nevergone::server_selection_view::kFallbackRowHeight;
    return false;
}

void rect_vertices(
        const server_selection_layout::RowRect& rect,
        GLfloat* vertices) {
    const int surface_width = g_surface_width.load(std::memory_order_relaxed);
    const int surface_height = g_surface_height.load(std::memory_order_relaxed);
    const auto mapping = nevergone::server_selection_view::mapping_for_surface(
        surface_width, surface_height);
    if (vertices == nullptr || !mapping.valid || !finite_rect(rect)) return;

    const float x0_px = mapping.offset_x + rect.left * mapping.scale;
    const float x1_px = mapping.offset_x + (rect.left + rect.width) * mapping.scale;
    const float y0_px = mapping.offset_y + rect.bottom * mapping.scale;
    const float y1_px = mapping.offset_y + (rect.bottom + rect.height) * mapping.scale;
    const float width = static_cast<float>(surface_width);
    const float height = static_cast<float>(surface_height);
    vertices[0] = 2.0f * x0_px / width - 1.0f;
    vertices[1] = 2.0f * y1_px / height - 1.0f;
    vertices[2] = vertices[0];
    vertices[3] = 2.0f * y0_px / height - 1.0f;
    vertices[4] = 2.0f * x1_px / width - 1.0f;
    vertices[5] = vertices[1];
    vertices[6] = vertices[4];
    vertices[7] = vertices[3];
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
    if (g_color_program == 0 || g_color_uniform < 0 || !mapping.valid || !finite_rect(rect)) return;
    GLfloat vertices[8]{};
    rect_vertices(rect, vertices);
    glUseProgram(g_color_program);
    glUniform4f(g_color_uniform, red, green, blue, alpha);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, vertices);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    glDisableVertexAttribArray(0);
}

void clear_row_texture() {
    if (g_row_texture != 0 && glIsTexture(g_row_texture) == GL_TRUE) {
        glDeleteTextures(1, &g_row_texture);
    }
    g_row_texture = 0;
    g_row_texture_generation = 0;
}

bool ensure_row_texture() {
    const std::uint64_t generation = nevergone::server_selection_assets::generation();
    if (!nevergone::server_selection_assets::row_ready()) {
        clear_row_texture();
        g_row_texture_generation = generation;
        return false;
    }
    if (g_row_texture_generation == generation && g_row_texture != 0 &&
            glIsTexture(g_row_texture) == GL_TRUE) {
        return true;
    }

    clear_row_texture();
    nevergone::server_selection_assets::Asset asset;
    if (!nevergone::server_selection_assets::copy(
            nevergone::server_selection_assets::kBorder2, &asset)) {
        g_row_texture_generation = generation;
        return false;
    }

    std::vector<std::uint8_t> rgba(asset.pixels.size() * 4u);
    for (std::size_t index = 0; index < asset.pixels.size(); ++index) {
        const std::uint32_t pixel = asset.pixels[index];
        rgba[index * 4u] = static_cast<std::uint8_t>((pixel >> 16u) & 0xffu);
        rgba[index * 4u + 1u] = static_cast<std::uint8_t>((pixel >> 8u) & 0xffu);
        rgba[index * 4u + 2u] = static_cast<std::uint8_t>(pixel & 0xffu);
        rgba[index * 4u + 3u] = static_cast<std::uint8_t>((pixel >> 24u) & 0xffu);
    }

    glGenTextures(1, &g_row_texture);
    if (g_row_texture == 0) return false;
    glBindTexture(GL_TEXTURE_2D, g_row_texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexImage2D(
        GL_TEXTURE_2D,
        0,
        GL_RGBA,
        asset.width,
        asset.height,
        0,
        GL_RGBA,
        GL_UNSIGNED_BYTE,
        rgba.data());
    const GLenum error = glGetError();
    glBindTexture(GL_TEXTURE_2D, 0);
    if (error != GL_NO_ERROR) {
        clear_row_texture();
        return false;
    }
    g_row_texture_generation = generation;
    return true;
}

void draw_row_texture(const server_selection_layout::RowRect& rect) {
    if (g_texture_program == 0 || g_texture_sampler < 0 || g_row_texture == 0 ||
            !finite_rect(rect)) {
        return;
    }
    GLfloat vertices[8]{};
    rect_vertices(rect, vertices);
    static constexpr GLfloat kTexCoords[] = {
        0.0f, 0.0f,
        0.0f, 1.0f,
        1.0f, 0.0f,
        1.0f, 1.0f,
    };
    const GLboolean blend_was_enabled = glIsEnabled(GL_BLEND);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glUseProgram(g_texture_program);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, g_row_texture);
    glUniform1i(g_texture_sampler, 0);
    glEnableVertexAttribArray(0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, vertices);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 0, kTexCoords);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    glDisableVertexAttribArray(1);
    glDisableVertexAttribArray(0);
    glBindTexture(GL_TEXTURE_2D, 0);
    if (blend_was_enabled == GL_FALSE) glDisable(GL_BLEND);
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
    g_color_program = 0;
    g_color_uniform = -1;
    g_texture_program = 0;
    g_texture_sampler = -1;
    g_row_texture = 0;
    g_row_texture_generation = 0;

    const GLuint color_program = build_color_program();
    if (color_program != 0) {
        const GLint color = glGetUniformLocation(color_program, "uColor");
        if (color >= 0) {
            g_color_program = color_program;
            g_color_uniform = color;
        } else {
            glDeleteProgram(color_program);
        }
    }

    const GLuint texture_program = build_texture_program();
    if (texture_program != 0) {
        const GLint sampler = glGetUniformLocation(texture_program, "uTexture");
        if (sampler >= 0) {
            g_texture_program = texture_program;
            g_texture_sampler = sampler;
        } else {
            glDeleteProgram(texture_program);
        }
    }
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
    if (!active() || g_color_program == 0) return;

    const auto state = nevergone::server_selection_state::snapshot();
    draw_rect(panel_rect(), 0.035f, 0.055f, 0.085f, 0.82f);

    float row_width = nevergone::server_selection_view::kFallbackRowWidth;
    float row_height = nevergone::server_selection_view::kFallbackRowHeight;
    const bool original_rows = row_dimensions(&row_width, &row_height) &&
        g_texture_program != 0 && ensure_row_texture();
    const auto rows = nevergone::server_selection_layout::build_rows(
        state.server_count, row_width, row_height);
    for (const auto& row : rows) {
        if (row.bottom + row.height < 0.0f ||
                row.bottom > server_selection_layout::kDesignHeight) {
            continue;
        }
        const bool selected = row.index == state.selected_index;
        if (original_rows) {
            draw_row_texture(row);
            // The shipped selection overlay/panel transition is not yet fully
            // recovered. Keep the existing project-owned highlight explicit.
            if (selected) draw_rect(row, 0.20f, 0.58f, 0.90f, 0.24f);
        } else if (selected) {
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

        float row_width = nevergone::server_selection_view::kFallbackRowWidth;
        float row_height = nevergone::server_selection_view::kFallbackRowHeight;
        (void)row_dimensions(&row_width, &row_height);
        const auto state = nevergone::server_selection_state::snapshot();
        const int hit_index = nevergone::server_selection_layout::hit_test(
            state.server_count,
            row_width,
            row_height,
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
    int row_width = 0;
    int row_height = 0;
    if (nevergone::server_selection_assets::dimensions(
            nevergone::server_selection_assets::kBorder2, &row_width, &row_height)) {
        out << "server selection row visual: original OBB border2 "
            << row_width << "x" << row_height << "\n";
    } else {
        out << "server selection row visual: project-owned fallback row="
            << nevergone::server_selection_view::kFallbackRowWidth
            << "x" << nevergone::server_selection_view::kFallbackRowHeight << "\n";
    }
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
