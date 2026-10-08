#include "choose_hero_black_cloud_compositor.h"

#include <GLES2/gl2.h>

#include <array>
#include <atomic>
#include <cstdint>
#include <sstream>
#include <vector>

#include "choose_hero_background_assets.h"
#include "choose_hero_black_cloud_timeline.h"
#include "game_clock.h"
#include "offline_startup_flow.h"

namespace nevergone::choose_hero_black_cloud_compositor {
namespace {

struct TextureAsset {
    GLuint texture = 0;
    std::uint64_t generation = 0;
    choose_hero_background::FrameAsset frame;
};

constexpr std::array<int, 3> kCloudAssetIndices{{7, 8, 9}};

GLuint g_program = 0;
GLint g_sampler = -1;
GLint g_alpha_uniform = -1;
std::array<TextureAsset, 3> g_textures{};
std::atomic<std::uint64_t> g_scene_generation{0};
std::atomic<std::uint64_t> g_start_tick{0};
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

void delete_texture(TextureAsset* asset) {
    if (asset == nullptr) return;
    if (asset->texture != 0 && glIsTexture(asset->texture) == GL_TRUE) {
        glDeleteTextures(1, &asset->texture);
    }
    *asset = TextureAsset{};
}

void delete_textures() {
    for (auto& asset : g_textures) delete_texture(&asset);
}

bool upload_texture(
        const choose_hero_background::FrameAsset& frame,
        GLuint* output) {
    if (output == nullptr || frame.width <= 0 || frame.height <= 0 ||
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

    *output = texture;
    return true;
}

bool ensure_textures() {
    const std::uint64_t generation = choose_hero_background::generation();
    bool current = true;
    for (const auto& asset : g_textures) {
        if (asset.texture == 0 || glIsTexture(asset.texture) != GL_TRUE ||
                asset.generation != generation) {
            current = false;
            break;
        }
    }
    if (current) return true;

    std::array<TextureAsset, 3> staged{};
    for (std::size_t index = 0; index < staged.size(); ++index) {
        if (!choose_hero_background::copy_frame(kCloudAssetIndices[index], &staged[index].frame) ||
                !upload_texture(staged[index].frame, &staged[index].texture)) {
            for (auto& asset : staged) delete_texture(&asset);
            return false;
        }
        staged[index].generation = generation;
    }

    delete_textures();
    g_textures = std::move(staged);
    return true;
}

const TextureAsset* texture_for_asset_index(int asset_index) {
    for (std::size_t index = 0; index < kCloudAssetIndices.size(); ++index) {
        if (kCloudAssetIndices[index] == asset_index) return &g_textures[index];
    }
    return nullptr;
}

void reset_scene_state() {
    g_scene_generation.store(0, std::memory_order_relaxed);
    g_start_tick.store(0, std::memory_order_relaxed);
    g_last_elapsed_seconds.store(0.0f, std::memory_order_relaxed);
    delete_textures();
}

void update_scene_start(const offline_startup_flow::Snapshot& route_state, std::uint64_t tick) {
    const std::uint64_t prior_generation = g_scene_generation.load(std::memory_order_relaxed);
    const std::uint64_t prior_start = g_start_tick.load(std::memory_order_relaxed);
    if (route_state.scene_generation != prior_generation || tick < prior_start) {
        g_scene_generation.store(route_state.scene_generation, std::memory_order_relaxed);
        g_start_tick.store(tick, std::memory_order_relaxed);
        g_last_elapsed_seconds.store(0.0f, std::memory_order_relaxed);
    }
}

void draw_cloud(
        const choose_hero_black_cloud_timeline::CloudPose& pose,
        const TextureAsset& asset,
        float scene_half_width,
        float scene_half_height) {
    const auto& frame = asset.frame;
    if (asset.texture == 0 || frame.source_width <= 0 || frame.source_height <= 0) return;

    // Cocos sprite position is expressed for the untrimmed source rectangle.
    // The recovered BalckCloud anchor is (1.0, 0.5), while TexturePacker left/
    // top locate the actual trimmed pixels inside that source rectangle.
    const float source_left = pose.x - pose.anchor_x * static_cast<float>(frame.source_width);
    const float source_bottom = pose.y - pose.anchor_y * static_cast<float>(frame.source_height);
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

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, asset.texture);
    glUniform1i(g_sampler, 0);
    glUniform1f(g_alpha_uniform, pose.alpha);
    glEnableVertexAttribArray(0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, vertices);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 0, kTexCoords);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    glDisableVertexAttribArray(1);
    glDisableVertexAttribArray(0);
}

}  // namespace

void draw() {
    const auto route_state = offline_startup_flow::snapshot();
    if (route_state.route != offline_startup_flow::Route::kChooseRole) {
        if (g_scene_generation.load(std::memory_order_relaxed) != 0) reset_scene_state();
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
    g_last_elapsed_seconds.store(static_cast<float>(elapsed_seconds), std::memory_order_relaxed);

    const auto& q01 = g_textures[0].frame;
    const auto& q02 = g_textures[1].frame;
    const auto& q03 = g_textures[2].frame;
    const auto poses = choose_hero_black_cloud_timeline::sample(
        elapsed_seconds,
        static_cast<float>(choose_hero_background::kDesignWidth),
        static_cast<float>(choose_hero_background::kDesignHeight),
        {static_cast<float>(q01.source_width), static_cast<float>(q01.source_height)},
        {static_cast<float>(q02.source_width), static_cast<float>(q02.source_height)},
        {static_cast<float>(q03.source_width), static_cast<float>(q03.source_height)});

    const float image_aspect = static_cast<float>(choose_hero_background::kDesignWidth) /
        static_cast<float>(choose_hero_background::kDesignHeight);
    const float surface_aspect = static_cast<float>(surface_width) /
        static_cast<float>(surface_height);
    float scene_half_width = 1.0f;
    float scene_half_height = 1.0f;
    if (image_aspect > surface_aspect) {
        scene_half_height = surface_aspect / image_aspect;
    } else {
        scene_half_width = image_aspect / surface_aspect;
    }

    glUseProgram(g_program);
    for (const auto& pose : poses) {
        const TextureAsset* asset = texture_for_asset_index(pose.asset_index);
        if (asset != nullptr) draw_cloud(pose, *asset, scene_half_width, scene_half_height);
    }
    glBindTexture(GL_TEXTURE_2D, 0);
    g_draw_count.fetch_add(1, std::memory_order_relaxed);
}

std::string status_report() {
    std::ostringstream out;
    const auto route_state = offline_startup_flow::snapshot();
    out << "ChooseHero BalckCloud compositor: "
        << (route_state.route == offline_startup_flow::Route::kChooseRole ? "active" : "inactive")
        << "\n";
    out << "ChooseHero cloud cycles: q02=40/20 q03=60/30 q01=50/25 seconds\n";
    out << "ChooseHero cloud generation: "
        << g_scene_generation.load(std::memory_order_relaxed) << " start="
        << g_start_tick.load(std::memory_order_relaxed) << "\n";
    out << "ChooseHero cloud elapsed: "
        << g_last_elapsed_seconds.load(std::memory_order_relaxed) << "\n";
    out << "ChooseHero cloud draws: "
        << g_draw_count.load(std::memory_order_relaxed) << "\n";
    return out.str();
}

}  // namespace nevergone::choose_hero_black_cloud_compositor
