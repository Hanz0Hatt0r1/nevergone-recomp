#include "choose_hero_background_compositor.h"

#include <GLES2/gl2.h>

#include <atomic>
#include <cstdint>
#include <sstream>
#include <vector>

#include "choose_hero_background_assets.h"
#include "choose_hero_background_timeline.h"
#include "game_clock.h"
#include "offline_startup_flow.h"

namespace nevergone::choose_hero_background_compositor {
namespace {

GLuint g_program = 0;
GLint g_sampler = -1;
GLint g_alpha_uniform = -1;
GLuint g_texture = 0;
std::uint64_t g_texture_generation = 0;

std::atomic<std::uint64_t> g_scene_generation{0};
std::atomic<std::uint64_t> g_start_tick{0};
std::atomic<std::uint64_t> g_draw_count{0};
std::atomic<std::uint64_t> g_uploaded_generation{0};
std::atomic<float> g_last_alpha{0.0f};

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

bool ensure_program() {
    if (g_program != 0 && glIsProgram(g_program) == GL_TRUE) return true;
    g_program = 0;
    g_sampler = -1;
    g_alpha_uniform = -1;

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
uniform float uAlpha;
void main() {
    vec4 color = texture2D(uTexture, vTexCoord);
    gl_FragColor = vec4(color.rgb, color.a * uAlpha);
}
)GLSL";

    const GLuint vertex = compile_shader(GL_VERTEX_SHADER, kVertex);
    const GLuint fragment = compile_shader(GL_FRAGMENT_SHADER, kFragment);
    if (vertex == 0 || fragment == 0) {
        if (vertex != 0) glDeleteShader(vertex);
        if (fragment != 0) glDeleteShader(fragment);
        return false;
    }

    const GLuint program = glCreateProgram();
    if (program == 0) {
        glDeleteShader(vertex);
        glDeleteShader(fragment);
        return false;
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
        return false;
    }

    const GLint sampler = glGetUniformLocation(program, "uTexture");
    const GLint alpha = glGetUniformLocation(program, "uAlpha");
    if (sampler < 0 || alpha < 0) {
        glDeleteProgram(program);
        return false;
    }

    g_program = program;
    g_sampler = sampler;
    g_alpha_uniform = alpha;
    return true;
}

void delete_texture() {
    if (g_texture != 0 && glIsTexture(g_texture) == GL_TRUE) {
        glDeleteTextures(1, &g_texture);
    }
    g_texture = 0;
    g_texture_generation = 0;
    g_uploaded_generation.store(0, std::memory_order_relaxed);
}

bool upload_texture(const choose_hero_background::FrameAsset& frame) {
    if (frame.width != choose_hero_background::kDesignWidth ||
            frame.height != choose_hero_background::kDesignHeight ||
            frame.source_width != choose_hero_background::kDesignWidth ||
            frame.source_height != choose_hero_background::kDesignHeight ||
            frame.left != 0 || frame.top != 0 ||
            frame.pixels.size() != static_cast<std::size_t>(frame.width) *
                    static_cast<std::size_t>(frame.height)) {
        return false;
    }

    const std::size_t count = frame.pixels.size();
    std::vector<std::uint8_t> rgba(count * 4u);
    for (std::size_t index = 0; index < count; ++index) {
        const std::uint32_t pixel = frame.pixels[index];
        rgba[index * 4u] = static_cast<std::uint8_t>((pixel >> 16u) & 0xffu);
        rgba[index * 4u + 1u] = static_cast<std::uint8_t>((pixel >> 8u) & 0xffu);
        rgba[index * 4u + 2u] = static_cast<std::uint8_t>(pixel & 0xffu);
        rgba[index * 4u + 3u] = static_cast<std::uint8_t>((pixel >> 24u) & 0xffu);
    }

    GLuint texture = 0;
    glGenTextures(1, &texture);
    if (texture == 0) return false;
    glBindTexture(GL_TEXTURE_2D, texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexImage2D(
        GL_TEXTURE_2D,
        0,
        GL_RGBA,
        frame.width,
        frame.height,
        0,
        GL_RGBA,
        GL_UNSIGNED_BYTE,
        rgba.data());
    const GLenum error = glGetError();
    glBindTexture(GL_TEXTURE_2D, 0);
    if (error != GL_NO_ERROR) {
        glDeleteTextures(1, &texture);
        return false;
    }

    delete_texture();
    g_texture = texture;
    return true;
}

bool ensure_texture() {
    const std::uint64_t asset_generation = choose_hero_background::generation();
    if (g_texture != 0 && glIsTexture(g_texture) == GL_TRUE &&
            g_texture_generation == asset_generation) {
        return true;
    }

    choose_hero_background::FrameAsset frame;
    if (!choose_hero_background::copy_frame(
            choose_hero_background::kStormBackgroundIndex,
            &frame)) {
        delete_texture();
        return false;
    }
    if (!upload_texture(frame)) return false;

    g_texture_generation = asset_generation;
    g_uploaded_generation.store(asset_generation, std::memory_order_relaxed);
    return true;
}

void reset_scene_state() {
    g_scene_generation.store(0, std::memory_order_relaxed);
    g_start_tick.store(0, std::memory_order_relaxed);
    g_last_alpha.store(0.0f, std::memory_order_relaxed);
    delete_texture();
}

void update_scene_start(const offline_startup_flow::Snapshot& route_state, std::uint64_t tick) {
    const std::uint64_t prior_generation = g_scene_generation.load(std::memory_order_relaxed);
    const std::uint64_t prior_start = g_start_tick.load(std::memory_order_relaxed);
    if (route_state.scene_generation != prior_generation || prior_start == 0 || tick < prior_start) {
        g_scene_generation.store(route_state.scene_generation, std::memory_order_relaxed);
        g_start_tick.store(tick, std::memory_order_relaxed);
        g_last_alpha.store(0.0f, std::memory_order_relaxed);
    }
}

}  // namespace

void draw() {
    const auto route_state = offline_startup_flow::snapshot();
    if (route_state.route != offline_startup_flow::Route::kChooseRole) {
        if (g_scene_generation.load(std::memory_order_relaxed) != 0 || g_texture != 0) {
            reset_scene_state();
        }
        return;
    }

    const std::uint64_t tick = game_clock::tick_count();
    update_scene_start(route_state, tick);
    if (!choose_hero_background::ready() || !ensure_program() || !ensure_texture()) return;

    GLint viewport[4]{};
    glGetIntegerv(GL_VIEWPORT, viewport);
    const int surface_width = viewport[2];
    const int surface_height = viewport[3];
    if (surface_width <= 0 || surface_height <= 0) return;

    const std::uint64_t start_tick = g_start_tick.load(std::memory_order_relaxed);
    const double elapsed_seconds = tick >= start_tick
        ? static_cast<double>(tick - start_tick) * game_clock::kFixedStepSeconds
        : 0.0;
    const float alpha = choose_hero_background_timeline::storm_alpha(elapsed_seconds);
    g_last_alpha.store(alpha, std::memory_order_relaxed);

    const float image_aspect = static_cast<float>(choose_hero_background::kDesignWidth) /
        static_cast<float>(choose_hero_background::kDesignHeight);
    const float surface_aspect = static_cast<float>(surface_width) /
        static_cast<float>(surface_height);
    float half_width = 1.0f;
    float half_height = 1.0f;
    if (image_aspect > surface_aspect) {
        half_height = surface_aspect / image_aspect;
    } else {
        half_width = image_aspect / surface_aspect;
    }

    const GLfloat vertices[] = {
        -half_width,  half_height,
        -half_width, -half_height,
         half_width,  half_height,
         half_width, -half_height,
    };
    static constexpr GLfloat kTexCoords[] = {
        0.0f, 0.0f,
        0.0f, 1.0f,
        1.0f, 0.0f,
        1.0f, 1.0f,
    };

    glUseProgram(g_program);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, g_texture);
    glUniform1i(g_sampler, 0);
    glUniform1f(g_alpha_uniform, alpha);
    glEnableVertexAttribArray(0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, vertices);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 0, kTexCoords);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    glDisableVertexAttribArray(1);
    glDisableVertexAttribArray(0);
    glBindTexture(GL_TEXTURE_2D, 0);
    g_draw_count.fetch_add(1, std::memory_order_relaxed);
}

std::string status_report() {
    std::ostringstream out;
    const auto route_state = offline_startup_flow::snapshot();
    out << "ChooseHero PartThree compositor: "
        << (route_state.route == offline_startup_flow::Route::kChooseRole ? "active" : "inactive")
        << "\n";
    out << "ChooseHero recovered visual: bejingwuyun FadeIn(6.0s)\n";
    out << "ChooseHero compositor generation: "
        << g_scene_generation.load(std::memory_order_relaxed) << "\n";
    out << "ChooseHero compositor start tick: "
        << g_start_tick.load(std::memory_order_relaxed) << "\n";
    out << "ChooseHero storm alpha: "
        << g_last_alpha.load(std::memory_order_relaxed) << "\n";
    out << "ChooseHero staged asset generation: "
        << choose_hero_background::generation() << " uploaded="
        << g_uploaded_generation.load(std::memory_order_relaxed) << "\n";
    out << "ChooseHero storm draws: "
        << g_draw_count.load(std::memory_order_relaxed) << "\n";
    return out.str();
}

}  // namespace nevergone::choose_hero_background_compositor
