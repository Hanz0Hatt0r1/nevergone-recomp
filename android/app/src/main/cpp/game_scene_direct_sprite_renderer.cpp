#include "game_scene_direct_sprite_renderer.h"

#include <GLES2/gl2.h>

#include <mutex>
#include <string>
#include <utility>
#include <vector>

#include "game_levels_runtime_state.h"
#include "game_scene_direct_asset_requests.h"
#include "game_scene_direct_sprite_geometry.h"
#include "game_scene_direct_texture_gl.h"

namespace nevergone::game_scene_direct_sprite_renderer {
namespace {

struct Drawable {
    game_scene_direct_texture_cache::Texture texture;
    game_scene_direct_sprite_geometry::Quad quad;
};

std::mutex g_mutex;
GLuint g_program = 0;
GLint g_sampler_uniform = -1;
Snapshot g_state;

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
    static constexpr const char* kVertexShader = R"GLSL(
attribute vec2 aPosition;
attribute vec2 aTexCoord;
varying vec2 vTexCoord;
void main() {
    vTexCoord = aTexCoord;
    gl_Position = vec4(aPosition, 0.0, 1.0);
}
)GLSL";
    static constexpr const char* kFragmentShader = R"GLSL(
precision mediump float;
varying vec2 vTexCoord;
uniform sampler2D uTexture;
void main() {
    gl_FragColor = texture2D(uTexture, vTexCoord);
}
)GLSL";

    const GLuint vertex = compile_shader(GL_VERTEX_SHADER, kVertexShader);
    if (vertex == 0) return 0;
    const GLuint fragment = compile_shader(GL_FRAGMENT_SHADER, kFragmentShader);
    if (fragment == 0) {
        glDeleteShader(vertex);
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

}  // namespace

void on_surface_created() {
    std::lock_guard<std::mutex> lock(g_mutex);
    // Any previous program name belonged to the old EGL context and is already
    // invalid there, so do not delete it through the newly-created context.
    g_program = build_program();
    g_sampler_uniform = g_program != 0
            ? glGetUniformLocation(g_program, "uTexture")
            : -1;
    if (g_program != 0 && g_sampler_uniform < 0) {
        glDeleteProgram(g_program);
        g_program = 0;
    }
    g_state = {};
    g_state.program_ready = g_program != 0;
}

std::size_t draw() {
    std::lock_guard<std::mutex> lock(g_mutex);
    ++g_state.draw_attempt_count;
    g_state.last_drawn_sprite_count = 0;
    g_state.last_texture_revision = 0;
    if (g_program == 0) return 0;

    const auto queue = game_levels_runtime_state::current_scene_render_queue();
    if (!queue.has_value() || queue->direct_file_count == 0u) return 0;

    const auto requests = game_scene_direct_asset_requests::build(*queue);
    const auto textures = game_scene_direct_texture_gl::snapshot();
    if (requests.requests.empty() ||
        textures.source_revision != requests.revision ||
        textures.texture_count != requests.requests.size()) {
        return 0;
    }

    std::vector<Drawable> drawables;
    drawables.reserve(requests.requests.size());
    for (const auto& request : requests.requests) {
        if (request.sprite_command_index >= queue->sprites.size()) return 0;
        const auto& command = queue->sprites[request.sprite_command_index];
        game_scene_direct_texture_cache::Texture texture;
        if (!game_scene_direct_texture_gl::texture_for_sprite_command(
                    request.sprite_command_index,
                    &texture)) {
            return 0;
        }
        game_scene_direct_sprite_geometry::Quad quad;
        if (!game_scene_direct_sprite_geometry::build(
                    command,
                    texture.width,
                    texture.height,
                    &quad)) {
            return 0;
        }
        drawables.push_back({texture, quad});
    }
    if (drawables.empty()) return 0;

    // The generic entered-game fallback is drawn immediately before this
    // renderer. Once a complete direct-sprite revision is ready, replace that
    // fallback with the reconstructed scene pixels.
    glClear(GL_COLOR_BUFFER_BIT);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glUseProgram(g_program);
    glUniform1i(g_sampler_uniform, 0);
    glActiveTexture(GL_TEXTURE0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glEnableVertexAttribArray(0);
    glEnableVertexAttribArray(1);

    for (const auto& drawable : drawables) {
        const auto* base = drawable.quad.vertices.data();
        glBindTexture(GL_TEXTURE_2D, static_cast<GLuint>(drawable.texture.handle));
        glVertexAttribPointer(
                0,
                2,
                GL_FLOAT,
                GL_FALSE,
                sizeof(game_scene_direct_sprite_geometry::Vertex),
                &base[0].x);
        glVertexAttribPointer(
                1,
                2,
                GL_FLOAT,
                GL_FALSE,
                sizeof(game_scene_direct_sprite_geometry::Vertex),
                &base[0].u);
        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    }

    glDisableVertexAttribArray(1);
    glDisableVertexAttribArray(0);
    glBindTexture(GL_TEXTURE_2D, 0);
    glUseProgram(0);

    g_state.last_drawn_sprite_count = drawables.size();
    g_state.last_texture_revision = textures.source_revision;
    return drawables.size();
}

Snapshot snapshot() {
    std::lock_guard<std::mutex> lock(g_mutex);
    return g_state;
}

}  // namespace nevergone::game_scene_direct_sprite_renderer
