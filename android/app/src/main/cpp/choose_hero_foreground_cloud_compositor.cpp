#include "choose_hero_foreground_cloud_compositor.h"

#include <GLES2/gl2.h>

#include <array>
#include <atomic>
#include <cstdint>
#include <sstream>
#include <vector>

#include "choose_hero_background_assets.h"
#include "choose_hero_foreground_cloud_timeline.h"
#include "game_clock.h"
#include "offline_startup_flow.h"

namespace nevergone::choose_hero_foreground_cloud_compositor {
namespace {

struct TextureSlot {
    int frame_index = -1;
    GLuint texture = 0;
    int width = 0;
    int height = 0;
    int left = 0;
    int top = 0;
    int source_width = 0;
    int source_height = 0;
};

GLuint g_program = 0;
GLint g_sampler = -1;
GLint g_alpha_uniform = -1;
std::array<TextureSlot, 3> g_slots{};
std::uint64_t g_texture_generation = 0;

std::atomic<std::uint64_t> g_scene_generation{0};
std::atomic<std::uint64_t> g_start_tick{0};
std::atomic<std::uint64_t> g_draw_count{0};
std::atomic<std::uint64_t> g_uploaded_generation{0};

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

void delete_textures() {
    for (TextureSlot& slot : g_slots) {
        if (slot.texture != 0 && glIsTexture(slot.texture) == GL_TRUE) {
            glDeleteTextures(1, &slot.texture);
        }
        slot = {};
    }
    g_texture_generation = 0;
    g_uploaded_generation.store(0, std::memory_order_relaxed);
}

bool upload_slot(std::size_t slot_index, int frame_index) {
    if (slot_index >= g_slots.size()) return false;

    choose_hero_background::FrameAsset frame;
    if (!choose_hero_background::copy_frame(frame_index, &frame) ||
            frame.width <= 0 || frame.height <= 0 ||
            frame.source_width <= 0 || frame.source_height <= 0 ||
            frame.left < 0 || frame.top < 0 ||
            frame.left + frame.width > frame.source_width ||
            frame.top + frame.height > frame.source_height ||
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

    TextureSlot& slot = g_slots[slot_index];
    slot.frame_index = frame_index;
    slot.texture = texture;
    slot.width = frame.width;
    slot.height = frame.height;
    slot.left = frame.left;
    slot.top = frame.top;
    slot.source_width = frame.source_width;
    slot.source_height = frame.source_height;
    return true;
}

bool ensure_textures() {
    const std::uint64_t asset_generation = choose_hero_background::generation();
    if (g_texture_generation == asset_generation) {
        for (const TextureSlot& slot : g_slots) {
            if (slot.texture == 0 || glIsTexture(slot.texture) != GL_TRUE) return false;
        }
        return true;
    }

    delete_textures();
    if (!upload_slot(0, 7) || !upload_slot(1, 8) || !upload_slot(2, 9)) {
        delete_textures();
        return false;
    }
    g_texture_generation = asset_generation;
    g_uploaded_generation.store(asset_generation, std::memory_order_relaxed);
    return true;
}

const TextureSlot* slot_for_frame(int frame_index) {
    for (const TextureSlot& slot : g_slots) {
        if (slot.frame_index == frame_index) return &slot;
    }
    return nullptr;
}

void reset_scene_state() {
    g_scene_generation.store(0, std::memory_order_relaxed);
    g_start_tick.store(0, std::memory_order_relaxed);
    delete_textures();
}

void update_scene_start(const offline_startup_flow::Snapshot& route_state, std::uint64_t tick) {
    const std::uint64_t prior_generation = g_scene_generation.load(std::memory_order_relaxed);
    const std::uint64_t prior_start = g_start_tick.load(std::memory_order_relaxed);
    if (route_state.scene_generation != prior_generation || tick < prior_start) {
        g_scene_generation.store(route_state.scene_generation, std::memory_order_relaxed);
        g_start_tick.store(tick, std::memory_order_relaxed);
    }
}

void draw_instance(
        const TextureSlot& slot,
        const choose_hero_foreground_cloud_timeline::Pose& pose,
        float half_width,
        float half_height) {
    const float source_width = static_cast<float>(slot.source_width);
    const float source_height = static_cast<float>(slot.source_height);
    const float canvas_left = pose.x - pose.anchor_x * source_width;
    const float canvas_bottom = pose.y - pose.anchor_y * source_height;

    const float pixel_left = canvas_left + static_cast<float>(slot.left);
    const float pixel_bottom = canvas_bottom +
        (source_height - static_cast<float>(slot.top + slot.height));
    const float pixel_right = pixel_left + static_cast<float>(slot.width);
    const float pixel_top = pixel_bottom + static_cast<float>(slot.height);

    const float x0 = -half_width + 2.0f * half_width *
        (pixel_left / choose_hero_foreground_cloud_timeline::kDesignWidth);
    const float x1 = -half_width + 2.0f * half_width *
        (pixel_right / choose_hero_foreground_cloud_timeline::kDesignWidth);
    const float y0 = -half_height + 2.0f * half_height *
        (pixel_bottom / choose_hero_foreground_cloud_timeline::kDesignHeight);
    const float y1 = -half_height + 2.0f * half_height *
        (pixel_top / choose_hero_foreground_cloud_timeline::kDesignHeight);

    const GLfloat vertices[] = {
        x0, y1,
        x0, y0,
        x1, y1,
        x1, y0,
    };
    static constexpr GLfloat kTexCoords[] = {
        0.0f, 0.0f,
        0.0f, 1.0f,
        1.0f, 0.0f,
        1.0f, 1.0f,
    };

    glBindTexture(GL_TEXTURE_2D, slot.texture);
    glUniform1f(g_alpha_uniform, pose.opacity);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, vertices);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 0, kTexCoords);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    g_draw_count.fetch_add(1, std::memory_order_relaxed);
}

}  // namespace

void draw() {
    const auto route_state = offline_startup_flow::snapshot();
    if (route_state.route != offline_startup_flow::Route::kChooseRole) {
        if (g_scene_generation.load(std::memory_order_relaxed) != 0 ||
                g_texture_generation != 0) {
            reset_scene_state();
        }
        return;
    }

    const std::uint64_t tick = game_clock::tick_count();
    update_scene_start(route_state, tick);
    if (!choose_hero_background::ready() || !ensure_program() || !ensure_textures()) return;

    GLint viewport[4]{};
    glGetIntegerv(GL_VIEWPORT, viewport);
    const int surface_width = viewport[2];
    const int surface_height = viewport[3];
    if (surface_width <= 0 || surface_height <= 0) return;

    const std::uint64_t start_tick = g_start_tick.load(std::memory_order_relaxed);
    const double elapsed_seconds = tick >= start_tick
        ? static_cast<double>(tick - start_tick) * game_clock::kFixedStepSeconds
        : 0.0;

    const float design_aspect = choose_hero_foreground_cloud_timeline::kDesignWidth /
        choose_hero_foreground_cloud_timeline::kDesignHeight;
    const float surface_aspect = static_cast<float>(surface_width) /
        static_cast<float>(surface_height);
    float half_width = 1.0f;
    float half_height = 1.0f;
    if (design_aspect > surface_aspect) {
        half_height = surface_aspect / design_aspect;
    } else {
        half_width = design_aspect / surface_aspect;
    }

    glUseProgram(g_program);
    glActiveTexture(GL_TEXTURE0);
    glUniform1i(g_sampler, 0);
    glEnableVertexAttribArray(0);
    glEnableVertexAttribArray(1);

    for (std::size_t index = 0;
            index < choose_hero_foreground_cloud_timeline::kInstanceCount;
            ++index) {
        // The frame choice is part of the recovered timeline. Find the staged
        // frame first so the Cocos content-size semantics use source dimensions.
        choose_hero_foreground_cloud_timeline::Pose probe;
        if (!choose_hero_foreground_cloud_timeline::pose_for_instance(
                index, elapsed_seconds, 1.0f, 1.0f, &probe)) {
            continue;
        }
        const TextureSlot* slot = slot_for_frame(probe.frame_index);
        if (slot == nullptr) continue;

        choose_hero_foreground_cloud_timeline::Pose pose;
        if (!choose_hero_foreground_cloud_timeline::pose_for_instance(
                index,
                elapsed_seconds,
                static_cast<float>(slot->source_width),
                static_cast<float>(slot->source_height),
                &pose)) {
            continue;
        }
        draw_instance(*slot, pose, half_width, half_height);
    }

    glDisableVertexAttribArray(1);
    glDisableVertexAttribArray(0);
    glBindTexture(GL_TEXTURE_2D, 0);
}

std::string status_report() {
    std::ostringstream out;
    const auto route_state = offline_startup_flow::snapshot();
    out << "ChooseHero foreground clouds: "
        << (route_state.route == offline_startup_flow::Route::kChooseRole ? "active" : "inactive")
        << "\n";
    out << "ChooseHero cloud sprites: 6 (qianjingyun02x2, qianjingyun03x2, qianjingyun01x2)\n";
    out << "ChooseHero cloud generation: "
        << g_scene_generation.load(std::memory_order_relaxed) << "\n";
    out << "ChooseHero cloud start tick: "
        << g_start_tick.load(std::memory_order_relaxed) << "\n";
    out << "ChooseHero cloud asset generation: "
        << choose_hero_background::generation() << " uploaded="
        << g_uploaded_generation.load(std::memory_order_relaxed) << "\n";
    out << "ChooseHero cloud draws: "
        << g_draw_count.load(std::memory_order_relaxed) << "\n";
    return out.str();
}

}  // namespace nevergone::choose_hero_foreground_cloud_compositor
