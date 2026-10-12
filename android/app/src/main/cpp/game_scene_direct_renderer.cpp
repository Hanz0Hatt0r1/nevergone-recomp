#include "game_scene_direct_renderer.h"

#include <GLES2/gl2.h>

#include <algorithm>
#include <atomic>
#include <cstdlib>
#include <sstream>
#include <string>
#include <vector>

#include "game_clock.h"
#include "game_levels_enter_transition.h"
#include "game_levels_runtime_state.h"
#include "game_scene_direct_asset_requests.h"
#include "game_scene_direct_geometry.h"
#include "game_scene_direct_texture_gl.h"
#include "game_scene_player_render_queue.h"
#include "game_scene_player_spawn_state.h"
#include "game_scene_type0_animation.h"

namespace nevergone::game_scene_direct_renderer {
namespace {

struct PreparedSprite {
    game_scene_direct_texture_cache::Texture texture;
    game_scene_direct_geometry::Quad quad;
    float opacity = 1.0f;
};

struct RuntimeAnimation {
    game_scene_type0_post_init::Behavior behavior =
            game_scene_type0_post_init::Behavior::kNone;
    game_scene_type0_post_init::RandomSkewLoopParams skew;
    game_scene_type0_post_init::RandomFadeLoopParams fade;
};

std::atomic<int> g_surface_width{0};
std::atomic<int> g_surface_height{0};
std::atomic<bool> g_shader_ready{false};
std::atomic<std::uint64_t> g_draw_attempt_count{0};
std::atomic<std::uint64_t> g_drawn_frame_count{0};
std::atomic<std::size_t> g_last_drawn_sprite_count{0};
std::atomic<std::size_t> g_last_animated_sprite_count{0};
std::atomic<std::uint8_t> g_last_min_opacity{255u};
std::atomic<float> g_last_camera_eye_x{0.0f};
GLuint g_program = 0;
GLint g_sampler_uniform = -1;
GLint g_opacity_uniform = -1;
std::string g_animation_scene_key;
std::uint64_t g_animation_start_tick = 0u;
std::vector<RuntimeAnimation> g_runtime_animations;

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
uniform float uOpacity;
void main() {
    gl_FragColor = texture2D(uTexture, vTexCoord) * uOpacity;
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

void clear_gl_errors() {
    while (glGetError() != GL_NO_ERROR) {
    }
}

std::string animation_scene_key(const game_scene_render_queue::Queue& queue) {
    std::ostringstream out;
    out << queue.source_scene_index << ':' << queue.guid << ':' << queue.sprites.size();
    return out.str();
}

std::uint32_t next_original_random_sample() {
    return static_cast<std::uint32_t>(::lrand48());
}

void reset_animation_runtime() {
    g_animation_scene_key.clear();
    g_animation_start_tick = 0u;
    g_runtime_animations.clear();
    g_last_animated_sprite_count.store(0u, std::memory_order_relaxed);
    g_last_min_opacity.store(255u, std::memory_order_relaxed);
}

void ensure_animation_runtime(const game_scene_render_queue::Queue& queue) {
    const std::string key = animation_scene_key(queue);
    if (key == g_animation_scene_key && g_runtime_animations.size() == queue.sprites.size()) {
        return;
    }

    g_animation_scene_key = key;
    g_animation_start_tick = game_clock::tick_count();
    g_runtime_animations.assign(queue.sprites.size(), RuntimeAnimation{});

    std::size_t animated_count = 0u;
    for (std::size_t index = 0; index < queue.sprites.size(); ++index) {
        const auto behavior = queue.sprites[index].post_init;
        auto& runtime = g_runtime_animations[index];
        runtime.behavior = behavior;
        switch (behavior) {
            case game_scene_type0_post_init::Behavior::kRandomSkewLoop:
                runtime.skew = game_scene_type0_post_init::make_random_skew_loop_params(
                        next_original_random_sample(), next_original_random_sample());
                ++animated_count;
                break;
            case game_scene_type0_post_init::Behavior::kRandomFadeLoop:
                runtime.fade = game_scene_type0_post_init::make_random_fade_loop_params(
                        next_original_random_sample(), next_original_random_sample());
                ++animated_count;
                break;
            case game_scene_type0_post_init::Behavior::kNone:
                break;
        }
    }
    g_last_animated_sprite_count.store(animated_count, std::memory_order_relaxed);
}

game_scene_type0_animation::Sample animation_sample(
        std::size_t sprite_command_index,
        std::uint64_t tick) {
    game_scene_type0_animation::Sample sample;
    if (sprite_command_index >= g_runtime_animations.size()) return sample;

    const double elapsed_seconds = tick >= g_animation_start_tick
        ? static_cast<double>(tick - g_animation_start_tick) * game_clock::kFixedStepSeconds
        : 0.0;
    const auto& runtime = g_runtime_animations[sprite_command_index];
    switch (runtime.behavior) {
        case game_scene_type0_post_init::Behavior::kRandomSkewLoop:
            return game_scene_type0_animation::evaluate_random_skew_loop(
                    runtime.skew, elapsed_seconds);
        case game_scene_type0_post_init::Behavior::kRandomFadeLoop:
            return game_scene_type0_animation::evaluate_random_fade_loop(
                    runtime.fade, elapsed_seconds);
        case game_scene_type0_post_init::Behavior::kNone:
            return sample;
    }
    return sample;
}

}  // namespace

void on_surface_created() {
    g_program = 0;
    g_sampler_uniform = -1;
    g_opacity_uniform = -1;
    g_shader_ready.store(false, std::memory_order_relaxed);
    g_last_drawn_sprite_count.store(0u, std::memory_order_relaxed);
    g_last_camera_eye_x.store(0.0f, std::memory_order_relaxed);
    reset_animation_runtime();

    g_program = build_program();
    if (g_program == 0) return;
    g_sampler_uniform = glGetUniformLocation(g_program, "uTexture");
    g_opacity_uniform = glGetUniformLocation(g_program, "uOpacity");
    if (g_sampler_uniform < 0 || g_opacity_uniform < 0) {
        glDeleteProgram(g_program);
        g_program = 0;
        g_sampler_uniform = -1;
        g_opacity_uniform = -1;
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
    g_last_min_opacity.store(255u, std::memory_order_relaxed);

    const int surface_width = g_surface_width.load(std::memory_order_relaxed);
    const int surface_height = g_surface_height.load(std::memory_order_relaxed);
    if (!route_active() || !g_shader_ready.load(std::memory_order_relaxed) ||
        g_program == 0 || g_sampler_uniform < 0 || g_opacity_uniform < 0 ||
        surface_width <= 0 || surface_height <= 0) {
        return false;
    }

    auto queue = game_levels_runtime_state::current_scene_render_queue();
    if (!queue.has_value()) return false;
    ensure_animation_runtime(*queue);
    const std::uint64_t tick = game_clock::tick_count();

    const auto spawn = game_scene_player_spawn_state::snapshot();
    const float camera_eye_x =
            game_scene_player_render_queue::current_initial_camera_eye_x(spawn);
    g_last_camera_eye_x.store(camera_eye_x, std::memory_order_relaxed);
    *queue = game_scene_player_render_queue::current(std::move(*queue));

    const auto requests = game_scene_direct_asset_requests::build(*queue);
    const auto order = game_scene_direct_geometry::ordered_texture_sprite_indices(*queue);
    if (order.empty() || requests.requests.size() != order.size()) return false;

    const auto texture_state = game_scene_direct_texture_gl::snapshot();
    if (texture_state.source_revision != requests.revision ||
        texture_state.texture_count != requests.requests.size()) {
        return false;
    }

    std::vector<PreparedSprite> prepared;
    prepared.reserve(order.size());
    std::uint8_t min_opacity = 255u;
    for (std::size_t index = 0; index < order.size(); ++index) {
        const std::size_t sprite_command_index = order[index];
        if (sprite_command_index >= queue->sprites.size() ||
            requests.requests[index].sprite_command_index != sprite_command_index) {
            return false;
        }

        game_scene_direct_texture_cache::Texture texture;
        if (!game_scene_direct_texture_gl::texture_for_sprite_command(
                    sprite_command_index, &texture)) {
            return false;
        }
        if (texture.hidden) continue;
        if (texture.handle == 0 || texture.width <= 0 || texture.height <= 0) return false;

        game_scene_direct_geometry::Quad quad;
        const auto& command = queue->sprites[sprite_command_index];
        const bool geometry_ready = command.player_body_transform.has_value()
            ? game_scene_direct_geometry::build_player_quad(
                    command.transform,
                    *command.player_body_transform,
                    texture.width,
                    texture.height,
                    surface_width,
                    surface_height,
                    &quad)
            : game_scene_direct_geometry::build_quad(
                    command.transform,
                    texture.width,
                    texture.height,
                    surface_width,
                    surface_height,
                    &quad);
        if (!geometry_ready) return false;

        const auto animated = animation_sample(sprite_command_index, tick);
        min_opacity = std::min(min_opacity, animated.opacity);
        prepared.push_back({texture, quad, static_cast<float>(animated.opacity) / 255.0f});
    }
    if (prepared.empty()) return false;

    clear_gl_errors();
    glClear(GL_COLOR_BUFFER_BIT);
    glEnable(GL_BLEND);
    glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
    glUseProgram(g_program);
    glActiveTexture(GL_TEXTURE0);
    glUniform1i(g_sampler_uniform, 0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glEnableVertexAttribArray(0);
    glEnableVertexAttribArray(1);

    for (const auto& sprite : prepared) {
        glUniform1f(g_opacity_uniform, sprite.opacity);
        glBindTexture(GL_TEXTURE_2D, static_cast<GLuint>(sprite.texture.handle));
        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, sprite.quad.positions.data());
        glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 0, sprite.quad.tex_coords.data());
        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    }

    glDisableVertexAttribArray(1);
    glDisableVertexAttribArray(0);
    glBindTexture(GL_TEXTURE_2D, 0);
    glUseProgram(0);

    if (glGetError() != GL_NO_ERROR) return false;

    g_last_min_opacity.store(min_opacity, std::memory_order_relaxed);
    g_last_drawn_sprite_count.store(prepared.size(), std::memory_order_relaxed);
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
    state.last_animated_sprite_count = g_last_animated_sprite_count.load(std::memory_order_relaxed);
    state.last_min_opacity = g_last_min_opacity.load(std::memory_order_relaxed);
    state.last_camera_eye_x = g_last_camera_eye_x.load(std::memory_order_relaxed);
    return state;
}

std::string status_report() {
    const Snapshot state = snapshot();
    std::ostringstream out;
    out << "GameScene type-0 renderer\n";
    out << "shader: " << (state.shader_ready ? "ready" : "unavailable") << "\n";
    out << "surface: " << state.surface_width << "x" << state.surface_height << "\n";
    out << "camera eye x: " << state.last_camera_eye_x << "\n";
    out << "draw attempts: " << state.draw_attempt_count << "\n";
    out << "drawn frames: " << state.drawn_frame_count << "\n";
    out << "last sprites: " << state.last_drawn_sprite_count << "\n";
    out << "post-init animated sprites: " << state.last_animated_sprite_count << "\n";
    out << "last minimum opacity: " << static_cast<unsigned int>(state.last_min_opacity) << "\n";
    return out.str();
}

}  // namespace nevergone::game_scene_direct_renderer
