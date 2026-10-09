#include "choose_hero_role_focus_compositor.h"

#include <GLES2/gl2.h>

#include <atomic>
#include <cstdint>
#include <sstream>
#include <vector>

#include "choose_hero_role_focus_asset.h"
#include "choose_hero_role_focus_timeline.h"
#include "choose_hero_role_item_assets.h"
#include "choose_hero_role_item_view.h"
#include "choose_hero_role_selection_state.h"
#include "game_clock.h"
#include "offline_startup_flow.h"

namespace nevergone::choose_hero_role_focus_compositor {
namespace {

GLuint g_program = 0;
GLint g_sampler = -1;
GLint g_alpha = -1;
GLuint g_texture = 0;
choose_hero_role_focus_asset::Asset g_asset;
std::uint64_t g_texture_generation = 0;

bool g_started = false;
std::uint64_t g_scene_generation = 0;
std::uint64_t g_selection_count = 0;
std::uint64_t g_start_tick = 0;
std::atomic<std::uint64_t> g_draw_count{0};
std::atomic<float> g_last_elapsed{0.0f};

bool route_active() {
    return offline_startup_flow::snapshot().route == offline_startup_flow::Route::kChooseRole;
}

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
    g_alpha = glGetUniformLocation(program, "uAlpha");
    if (g_sampler < 0 || g_alpha < 0) {
        glDeleteProgram(program);
        return false;
    }
    g_program = program;
    return true;
}

void delete_texture() {
    if (g_texture != 0 && glIsTexture(g_texture) == GL_TRUE) glDeleteTextures(1, &g_texture);
    g_texture = 0;
    g_asset = {};
    g_texture_generation = 0;
}

bool ensure_texture() {
    const std::uint64_t generation = choose_hero_role_focus_asset::generation();
    if (g_texture_generation == generation && g_texture != 0 && glIsTexture(g_texture) == GL_TRUE) {
        return true;
    }
    delete_texture();
    choose_hero_role_focus_asset::Asset asset;
    if (!choose_hero_role_focus_asset::copy(&asset)) return false;
    std::vector<std::uint8_t> rgba(asset.pixels.size() * 4u);
    for (std::size_t i = 0; i < asset.pixels.size(); ++i) {
        const std::uint32_t pixel = asset.pixels[i];
        rgba[i * 4u] = static_cast<std::uint8_t>((pixel >> 16u) & 0xffu);
        rgba[i * 4u + 1u] = static_cast<std::uint8_t>((pixel >> 8u) & 0xffu);
        rgba[i * 4u + 2u] = static_cast<std::uint8_t>(pixel & 0xffu);
        rgba[i * 4u + 3u] = static_cast<std::uint8_t>((pixel >> 24u) & 0xffu);
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
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, asset.width, asset.height, 0,
                 GL_RGBA, GL_UNSIGNED_BYTE, rgba.data());
    const GLenum error = glGetError();
    glBindTexture(GL_TEXTURE_2D, 0);
    if (error != GL_NO_ERROR) {
        glDeleteTextures(1, &texture);
        return false;
    }
    g_texture = texture;
    g_asset = std::move(asset);
    g_texture_generation = generation;
    return true;
}

void draw_highlight(
        float center_x,
        float center_y,
        float alpha,
        const choose_hero_role_item_view::Mapping& mapping,
        int surface_width,
        int surface_height) {
    if (alpha <= 0.0f || g_texture == 0) return;
    const float half_width = static_cast<float>(g_asset.width) * 0.5f;
    const float half_height = static_cast<float>(g_asset.height) * 0.5f;
    const float left_px = mapping.offset_x + (center_x - half_width) * mapping.scale;
    const float right_px = mapping.offset_x + (center_x + half_width) * mapping.scale;
    const float bottom_px = mapping.offset_y + (center_y - half_height) * mapping.scale;
    const float top_px = mapping.offset_y + (center_y + half_height) * mapping.scale;
    const float sw = static_cast<float>(surface_width);
    const float sh = static_cast<float>(surface_height);
    const GLfloat x0 = 2.0f * left_px / sw - 1.0f;
    const GLfloat x1 = 2.0f * right_px / sw - 1.0f;
    const GLfloat y0 = 2.0f * bottom_px / sh - 1.0f;
    const GLfloat y1 = 2.0f * top_px / sh - 1.0f;
    const GLfloat vertices[] = {x0,y1, x0,y0, x1,y1, x1,y0};
    static constexpr GLfloat kTexCoords[] = {0,0, 0,1, 1,0, 1,1};
    glBindTexture(GL_TEXTURE_2D, g_texture);
    glUniform1f(g_alpha, alpha);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, vertices);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 0, kTexCoords);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    g_draw_count.fetch_add(1, std::memory_order_relaxed);
}

void reset_runtime() {
    g_started = false;
    g_scene_generation = 0;
    g_selection_count = 0;
    g_start_tick = 0;
    g_last_elapsed.store(0.0f, std::memory_order_relaxed);
    delete_texture();
}

}  // namespace

void draw() {
    if (!route_active()) {
        if (g_started || g_texture != 0) reset_runtime();
        return;
    }
    if (!choose_hero_role_item_assets::ready() || !choose_hero_role_focus_asset::ready()) return;

    const auto state = choose_hero_role_selection_state::snapshot();
    std::size_t selected_index = state.items.size();
    for (std::size_t i = 0; i < state.items.size(); ++i) {
        if (state.items[i].selected) {
            selected_index = i;
            break;
        }
    }
    if (selected_index == state.items.size()) return;

    const std::uint64_t tick = game_clock::tick_count();
    if (!g_started || g_scene_generation != state.scene_generation ||
            g_selection_count != state.selection_count || tick < g_start_tick) {
        g_started = true;
        g_scene_generation = state.scene_generation;
        g_selection_count = state.selection_count;
        g_start_tick = tick;
    }

    choose_hero_role_item_assets::Asset board;
    if (!choose_hero_role_item_assets::copy(choose_hero_role_item_assets::kBoardA, &board)) return;
    choose_hero_role_selection_state::ItemPose item_pose;
    if (!choose_hero_role_selection_state::item_pose(
            selected_index,
            choose_hero_role_item_view::kDesignHeight,
            static_cast<float>(board.height),
            &item_pose)) return;

    if (!ensure_program() || !ensure_texture()) return;
    GLint viewport[4]{};
    glGetIntegerv(GL_VIEWPORT, viewport);
    const int surface_width = viewport[2];
    const int surface_height = viewport[3];
    if (surface_width <= 0 || surface_height <= 0) return;
    const auto mapping = choose_hero_role_item_view::mapping_for_surface(surface_width, surface_height);
    if (!mapping.valid) return;

    const double elapsed = tick >= g_start_tick
        ? static_cast<double>(tick - g_start_tick) * game_clock::kFixedStepSeconds
        : 0.0;
    g_last_elapsed.store(static_cast<float>(elapsed), std::memory_order_relaxed);

    glUseProgram(g_program);
    glActiveTexture(GL_TEXTURE0);
    glUniform1i(g_sampler, 0);
    glEnableVertexAttribArray(0);
    glEnableVertexAttribArray(1);

    const float board_left = item_pose.x - static_cast<float>(board.width) * 0.5f;
    const float board_bottom = item_pose.y - static_cast<float>(board.height) * 0.5f;
    for (std::size_t index = 0; index < 2; ++index) {
        choose_hero_role_focus_timeline::HighlightPose pose;
        if (choose_hero_role_focus_timeline::sample(
                index,
                elapsed,
                static_cast<float>(board.width),
                static_cast<float>(board.height),
                static_cast<float>(g_asset.width),
                &pose)) {
            draw_highlight(
                board_left + pose.x,
                board_bottom + pose.y,
                pose.alpha,
                mapping,
                surface_width,
                surface_height);
        }
    }

    glDisableVertexAttribArray(1);
    glDisableVertexAttribArray(0);
    glBindTexture(GL_TEXTURE_2D, 0);
}

std::string status_report() {
    std::ostringstream out;
    out << "ChooseHero focus highlight: " << (route_active() ? "active" : "inactive") << "\n";
    out << "ChooseHero focus asset: "
        << (choose_hero_role_focus_asset::ready() ? "ready" : "optional-missing") << "\n";
    out << "ChooseHero focus generation/selection: " << g_scene_generation
        << "/" << g_selection_count << " start=" << g_start_tick << "\n";
    out << "ChooseHero focus elapsed/draws: "
        << g_last_elapsed.load(std::memory_order_relaxed) << "/"
        << g_draw_count.load(std::memory_order_relaxed) << "\n";
    return out.str();
}

}  // namespace nevergone::choose_hero_role_focus_compositor
