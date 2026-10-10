#include "game_scene_direct_renderer.h"

#include <GLES2/gl2.h>

#include <atomic>
#include <sstream>

#include "game_levels_enter_transition.h"
#include "game_levels_runtime_state.h"
#include "game_scene_direct_geometry.h"
#include "game_scene_direct_texture_gl.h"

namespace nevergone::game_scene_direct_renderer {
namespace {

std::atomic<int> g_surface_width{0};
std::atomic<int> g_surface_height{0};
std::atomic<bool> g_shader_ready{false};
std::atomic<std::uint64_t> g_draw_attempt_count{0};
std::atomic<std::uint64_t> g_drawn_frame_count{0};
std::atomic<std::size_t> g_last_drawn_sprite_count{0};
GLuint g_program = 0;
GLint g_sampler_uniform = -1;

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
    return game_levels_enter_transition::snapshot().boundary ==
        game_levels_enter_transition::Boundary::kRuntimeRenderQueueReady;
}

}  // namespace

void on_surface_created() {
    // The previous program belonged to the old EGL context. Drop the opaque
    // handle without deleting it through the new context and rebuild locally.
    g_program = 0;
    g_sampler_uniform = -1;
    g_shader_ready.store(false, std::memory_order_relaxed);
    g_last_drawn_sprite_count.store(0u, std::memory_order_relaxed);

    g_program = build_program();
    if (g_program == 0) return;
    g_sampler_uniform = glGetUniformLocation(g_program, "uTexture");
    if (g_sampler_uniform < 0) {
        glDeleteProgram(g_program);
        g_program = 0;
        return;
    }
    g_shader_ready.store(true, std::memory_order_relaxed);
}

void on_surface_changed(int width, int height) {
    g_surface_width.store(width, std::memory_order_relaxed);
    g_surface_height.store(height, std::memory_order_relaxed);
}

bool draw() {
    g_draw_attempt_count.fetch_add(1u, std::memory_order_relaxed);
    g_last_drawn_sprite_count.store(0u, std::memory_order_relaxed);

    const int surface_width = g_surface_width.load(std::memory_order_relaxed);
    const int surface_height = g_surface_height.load(std::memory_order_relaxed);
    if (!route_active() || !g_shader_ready.load(std::memory_order_relaxed) ||
        g_program == 0 || g_sampler_uniform < 0 ||
        surface_width <= 0 || surface_height <= 0) {
        return false;
    }

    const auto queue = game_levels_runtime_state::current_scene_render_queue();
    if (!queue.has_value()) return false;
    const auto order = game_scene_direct_geometry::ordered_direct_sprite_indices(*queue);
    if (order.empty()) return false;

    std::size_t drawn = 0u;
    glUseProgram(g_program);
    glActiveTexture(GL_TEXTURE0);
    glUniform1i(g_sampler_uniform, 0);
    glEnableVertexAttribArray(0);
    glEnableVertexAttribArray(1);

    for (const std::size_t sprite_command_index : order) {
        game_scene_direct_texture_cache::Texture texture;
        if (!game_scene_direct_texture_gl::texture_for_sprite_command(
                sprite_command_index, &texture) ||
            texture.handle == 0 || texture.width <= 0 || texture.height <= 0) {
            continue;
        }

        game_scene_direct_geometry::Quad quad;
        if (!game_scene_direct_geometry::build_quad(
                queue->sprites[sprite_command_index].transform,
                texture.width,
                texture.height,
                surface_width,
                surface_height,
                &quad)) {
            continue;
        }

        glBindTexture(GL_TEXTURE_2D, static_cast<GLuint>(texture.handle));
        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, quad.positions.data());
        glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 0, quad.tex_coords.data());
        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
        ++drawn;
    }

    glDisableVertexAttribArray(1);
    glDisableVertexAttribArray(0);
    glBindTexture(GL_TEXTURE_2D, 0);

    g_last_drawn_sprite_count.store(drawn, std::memory_order_relaxed);
    if (drawn == 0u) return false;
    g_drawn_frame_count.fetch_add(1u, std::memory_order_relaxed);
    return true;
}

Snapshot snapshot() {
    Snapshot state;
    state.shader_ready = g_shader_ready.load(std::memory_order_relaxed);
    state.surface_width = g_surface_width.load(std::memory_order_relaxed);
    state.surface_height = g_surface_height.load(std::memory_order_relaxed);
    state.draw_attempt_count = g_draw_attempt_count.load(std::memory_order_relaxed);
    state.drawn_frame_count = g_drawn_frame_count.load(std::memory_order_relaxed);
    state.last_drawn_sprite_count = g_last_drawn_sprite_count.load(std::memory_order_relaxed);
    return state;
}

std::string status_report() {
    const Snapshot state = snapshot();
    std::ostringstream out;
    out << "GameScene direct renderer\n";
    out << "shader: " << (state.shader_ready ? "ready" : "unavailable") << "\n";
    out << "surface: " << state.surface_width << "x" << state.surface_height << "\n";
    out << "draw attempts: " << state.draw_attempt_count << "\n";
    out << "drawn frames: " << state.drawn_frame_count << "\n";
    out << "last direct sprites: " << state.last_drawn_sprite_count << "\n";
    return out.str();
}

}  // namespace nevergone::game_scene_direct_renderer
