#include "choose_hero_thunder_effect_compositor.h"

#include <GLES2/gl2.h>

#include <array>
#include <atomic>
#include <cstdint>
#include <ctime>
#include <mutex>
#include <sstream>
#include <vector>

#include "choose_hero_background_assets.h"
#include "choose_hero_thunder_state.h"
#include "game_clock.h"
#include "offline_startup_flow.h"

namespace nevergone::choose_hero_thunder_effect_compositor {
namespace {

constexpr std::size_t kTextureCount = 13;

struct TextureAsset {
    int frame_index = -1;
    GLuint texture = 0;
    choose_hero_background::FrameAsset frame;
};

GLuint g_program = 0;
GLint g_sampler = -1;
GLint g_alpha_uniform = -1;
std::array<TextureAsset, kTextureCount> g_textures{};
std::uint64_t g_texture_generation = 0;

std::mutex g_state_mutex;
choose_hero_thunder_state::Machine g_machine;
bool g_scene_active = false;
std::uint64_t g_scene_generation = 0;
std::uint64_t g_start_tick = 0;
std::uint64_t g_seed_seconds = 0;
std::uint64_t g_suppressed_sound_count = 0;
int g_last_suppressed_sound = -1;

std::atomic<std::uint64_t> g_draw_count{0};
std::atomic<float> g_last_elapsed_seconds{0.0f};

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
    g_sampler = glGetUniformLocation(program, "uTexture");
    g_alpha_uniform = glGetUniformLocation(program, "uAlpha");
    if (g_sampler < 0 || g_alpha_uniform < 0) {
        glDeleteProgram(program);
        g_sampler = -1;
        g_alpha_uniform = -1;
        return false;
    }
    g_program = program;
    return true;
}

void delete_textures() {
    for (TextureAsset& asset : g_textures) {
        if (asset.texture != 0 && glIsTexture(asset.texture) == GL_TRUE) {
            glDeleteTextures(1, &asset.texture);
        }
        asset = {};
    }
    g_texture_generation = 0;
}

bool upload_texture(TextureAsset* asset, int frame_index) {
    if (asset == nullptr) return false;
    choose_hero_background::FrameAsset frame;
    if (!choose_hero_background::copy_frame(frame_index, &frame)) return false;
    if (frame.width <= 0 || frame.height <= 0 ||
            frame.source_width <= 0 || frame.source_height <= 0 ||
            frame.left < 0 || frame.top < 0 ||
            frame.left + frame.width > frame.source_width ||
            frame.top + frame.height > frame.source_height ||
            frame.pixels.size() != static_cast<std::size_t>(frame.width) *
                    static_cast<std::size_t>(frame.height)) {
        return false;
    }

    std::vector<std::uint8_t> rgba(frame.pixels.size() * 4u);
    for (std::size_t index = 0; index < frame.pixels.size(); ++index) {
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
        GL_TEXTURE_2D, 0, GL_RGBA, frame.width, frame.height, 0,
        GL_RGBA, GL_UNSIGNED_BYTE, rgba.data());
    const GLenum error = glGetError();
    glBindTexture(GL_TEXTURE_2D, 0);
    if (error != GL_NO_ERROR) {
        glDeleteTextures(1, &texture);
        return false;
    }

    asset->frame_index = frame_index;
    asset->texture = texture;
    asset->frame = std::move(frame);
    return true;
}

bool textures_valid() {
    for (const TextureAsset& asset : g_textures) {
        if (asset.texture == 0 || glIsTexture(asset.texture) != GL_TRUE) return false;
    }
    return true;
}

bool ensure_textures() {
    const std::uint64_t generation = choose_hero_background::generation();
    if (g_texture_generation == generation && textures_valid()) return true;

    delete_textures();
    std::size_t slot = 0;
    for (int index = 0; index < choose_hero_background::kEffectFrameCount; ++index) {
        if (!upload_texture(
                &g_textures[slot++],
                choose_hero_background::kLightningFirstIndex + index)) {
            delete_textures();
            return false;
        }
    }
    for (int index = 0; index < choose_hero_background::kEffectFrameCount; ++index) {
        if (!upload_texture(
                &g_textures[slot++],
                choose_hero_background::kThunderFirstIndex + index)) {
            delete_textures();
            return false;
        }
    }
    if (!upload_texture(&g_textures[slot], choose_hero_background::kGroundLightIndex)) {
        delete_textures();
        return false;
    }
    g_texture_generation = generation;
    return true;
}

void reset_scene_state() {
    std::lock_guard<std::mutex> lock(g_state_mutex);
    g_scene_active = false;
    g_scene_generation = 0;
    g_start_tick = 0;
    g_seed_seconds = 0;
    g_suppressed_sound_count = 0;
    g_last_suppressed_sound = -1;
    g_machine.reset(0);
    delete_textures();
    g_last_elapsed_seconds.store(0.0f, std::memory_order_relaxed);
}

choose_hero_thunder_state::Snapshot update_state(
        const offline_startup_flow::Snapshot& route_state,
        std::uint64_t tick,
        double* elapsed_out) {
    std::lock_guard<std::mutex> lock(g_state_mutex);
    if (!g_scene_active || route_state.scene_generation != g_scene_generation || tick < g_start_tick) {
        g_scene_active = true;
        g_scene_generation = route_state.scene_generation;
        g_start_tick = tick;
        const std::time_t now = std::time(nullptr);
        g_seed_seconds = now > 0 ? static_cast<std::uint64_t>(now) : route_state.scene_generation;
        g_machine.reset(g_seed_seconds);
        g_machine.start();
        g_suppressed_sound_count = 0;
        g_last_suppressed_sound = -1;
    }

    const double elapsed = tick >= g_start_tick
        ? static_cast<double>(tick - g_start_tick) * game_clock::kFixedStepSeconds
        : 0.0;
    g_machine.advance(elapsed);
    for (int sound = g_machine.take_sound_index(); sound >= 0; sound = g_machine.take_sound_index()) {
        ++g_suppressed_sound_count;
        g_last_suppressed_sound = sound;
    }
    if (elapsed_out != nullptr) *elapsed_out = elapsed;
    return g_machine.snapshot(elapsed);
}

void draw_centered(
        const TextureAsset& asset,
        float alpha,
        float scene_half_width,
        float scene_half_height) {
    if (alpha <= 0.0f || asset.texture == 0) return;
    const auto& frame = asset.frame;
    const float source_width = static_cast<float>(frame.source_width);
    const float source_height = static_cast<float>(frame.source_height);
    const float source_left = static_cast<float>(choose_hero_background::kDesignWidth) * 0.5f -
        source_width * 0.5f;
    const float source_bottom = static_cast<float>(choose_hero_background::kDesignHeight) * 0.5f -
        source_height * 0.5f;
    const float trimmed_left = source_left + static_cast<float>(frame.left);
    const float trimmed_bottom = source_bottom +
        static_cast<float>(frame.source_height - frame.top - frame.height);
    const float trimmed_right = trimmed_left + static_cast<float>(frame.width);
    const float trimmed_top = trimmed_bottom + static_cast<float>(frame.height);

    const float design_width = static_cast<float>(choose_hero_background::kDesignWidth);
    const float design_height = static_cast<float>(choose_hero_background::kDesignHeight);
    const GLfloat x0 = -scene_half_width + 2.0f * scene_half_width * trimmed_left / design_width;
    const GLfloat x1 = -scene_half_width + 2.0f * scene_half_width * trimmed_right / design_width;
    const GLfloat y0 = -scene_half_height + 2.0f * scene_half_height * trimmed_bottom / design_height;
    const GLfloat y1 = -scene_half_height + 2.0f * scene_half_height * trimmed_top / design_height;
    const GLfloat vertices[] = {x0, y1, x0, y0, x1, y1, x1, y0};
    static constexpr GLfloat kTexCoords[] = {
        0.0f, 0.0f,
        0.0f, 1.0f,
        1.0f, 0.0f,
        1.0f, 1.0f,
    };

    glBindTexture(GL_TEXTURE_2D, asset.texture);
    glUniform1f(g_alpha_uniform, alpha);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, vertices);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 0, kTexCoords);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    g_draw_count.fetch_add(1, std::memory_order_relaxed);
}

}  // namespace

void draw() {
    const auto route_state = offline_startup_flow::snapshot();
    if (route_state.route != offline_startup_flow::Route::kChooseRole) {
        if (g_scene_active || g_texture_generation != 0) reset_scene_state();
        return;
    }

    const std::uint64_t tick = game_clock::tick_count();
    double elapsed_seconds = 0.0;
    const auto state = update_state(route_state, tick, &elapsed_seconds);
    g_last_elapsed_seconds.store(static_cast<float>(elapsed_seconds), std::memory_order_relaxed);

    if (!choose_hero_background::ready() ||
            !choose_hero_background::effects_ready() ||
            !ensure_program() || !ensure_textures()) {
        return;
    }

    GLint viewport[4]{};
    glGetIntegerv(GL_VIEWPORT, viewport);
    if (viewport[2] <= 0 || viewport[3] <= 0) return;
    const float image_aspect = static_cast<float>(choose_hero_background::kDesignWidth) /
        static_cast<float>(choose_hero_background::kDesignHeight);
    const float surface_aspect = static_cast<float>(viewport[2]) / static_cast<float>(viewport[3]);
    float scene_half_width = 1.0f;
    float scene_half_height = 1.0f;
    if (image_aspect > surface_aspect) {
        scene_half_height = surface_aspect / image_aspect;
    } else {
        scene_half_width = image_aspect / surface_aspect;
    }

    glUseProgram(g_program);
    glActiveTexture(GL_TEXTURE0);
    glUniform1i(g_sampler, 0);
    glEnableVertexAttribArray(0);
    glEnableVertexAttribArray(1);

    // Same-z Cocos insertion order from PartThree(): six shandian sprites,
    // then six menlei sprites, then the centered ground-light sprite.
    for (std::size_t index = 0; index < choose_hero_thunder_state::kEffectCount; ++index) {
        draw_centered(g_textures[index], state.lightning_alpha[index], scene_half_width, scene_half_height);
    }
    for (std::size_t index = 0; index < choose_hero_thunder_state::kEffectCount; ++index) {
        draw_centered(
            g_textures[choose_hero_thunder_state::kEffectCount + index],
            state.thunder_alpha[index],
            scene_half_width,
            scene_half_height);
    }
    draw_centered(g_textures.back(), state.ground_light_alpha, scene_half_width, scene_half_height);

    glDisableVertexAttribArray(1);
    glDisableVertexAttribArray(0);
    glBindTexture(GL_TEXTURE_2D, 0);
}

std::string status_report() {
    std::lock_guard<std::mutex> lock(g_state_mutex);
    const auto state = g_machine.snapshot(g_last_elapsed_seconds.load(std::memory_order_relaxed));
    std::ostringstream out;
    out << "ChooseHero thunder effects: " << (g_scene_active ? "active" : "inactive") << "\n";
    out << "ChooseHero thunder seed: " << g_seed_seconds << "\n";
    out << "ChooseHero thunder elapsed: "
        << g_last_elapsed_seconds.load(std::memory_order_relaxed) << "\n";
    out << "ChooseHero thunder RNG draws: " << state.rng_draw_count << "\n";
    out << "ChooseHero thunder begin/end: "
        << state.thunder_begin_count << "/" << state.thunder_end_count << "\n";
    out << "ChooseHero lightning schedules: " << state.lightning_schedule_count << "\n";
    out << "ChooseHero ground-light schedules: " << state.ground_light_schedule_count << "\n";
    out << "ChooseHero suppressed thunder sounds: " << g_suppressed_sound_count
        << " last=" << g_last_suppressed_sound << "\n";
    out << "ChooseHero effect draws: " << g_draw_count.load(std::memory_order_relaxed) << "\n";
    return out.str();
}

}  // namespace nevergone::choose_hero_thunder_effect_compositor
