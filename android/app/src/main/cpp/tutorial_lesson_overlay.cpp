#include "tutorial_lesson_overlay.h"

#include <GLES2/gl2.h>
#include <jni.h>

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <mutex>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include "login_localization_csv.h"
#include "tutorial_gameplay_state.h"
#include "tutorial_session_state.h"

namespace nevergone::tutorial_lesson_overlay {
namespace {

constexpr float kDesignWidth = 1136.0f;
constexpr float kDesignHeight = 640.0f;
constexpr float kContinueCenterX = kDesignWidth - 160.0f;
constexpr float kContinueCenterY = 45.0f;
constexpr float kPromptCenterX = kDesignWidth - 150.0f;
constexpr float kPromptCenterY = kDesignHeight - 60.0f;
constexpr int kActionDown = 0;
constexpr int kActionUp = 1;
constexpr int kActionMove = 2;
constexpr int kActionCancel = 3;
constexpr int kActionPointerDown = 5;
constexpr int kActionPointerUp = 6;
constexpr std::size_t kMaxPixels = 16u * 1024u * 1024u;

struct Asset {
    int width = 0;
    int height = 0;
    std::vector<std::uint32_t> pixels;
};

struct State {
    Asset keyboard;
    Asset continue_label;
    Asset prompt_board;
    Asset prompt_titlebar;
    Asset prompt_title;
    Asset prompt_body;
    Asset prompt_hints;
    std::uint64_t asset_generation = 0;
    std::uint64_t prompt_generation = 0;
    int prompt_step = -1;
    std::uint64_t draw_attempt_count = 0;
    std::uint64_t drawn_frame_count = 0;
    std::uint64_t accepted_begin_count = 0;
    std::uint64_t accepted_end_count = 0;
    std::uint64_t continue_tap_count = 0;
    std::uint64_t completion_count = 0;
    std::uint64_t cancel_count = 0;
    int captured_pointer = -1;
};

struct SurfaceMetrics {
    int width = 0;
    int height = 0;
    float scale = 0.0f;
    float offset_x = 0.0f;
    float offset_y = 0.0f;
};

std::mutex g_mutex;
State g_state;
GLuint g_program = 0;
GLint g_sampler = -1;
GLuint g_keyboard_texture = 0;
GLuint g_continue_texture = 0;
GLuint g_prompt_board_texture = 0;
GLuint g_prompt_titlebar_texture = 0;
GLuint g_prompt_title_texture = 0;
GLuint g_prompt_body_texture = 0;
GLuint g_prompt_hints_texture = 0;
std::uint64_t g_texture_generation = 0;
std::uint64_t g_prompt_texture_generation = 0;
std::atomic<int> g_surface_width{0};
std::atomic<int> g_surface_height{0};

bool valid_asset(const Asset& asset) {
    return asset.width > 0 && asset.height > 0 &&
        asset.pixels.size() ==
            static_cast<std::size_t>(asset.width) * static_cast<std::size_t>(asset.height) &&
        asset.pixels.size() <= kMaxPixels;
}

bool make_asset(
        int width,
        int height,
        const std::uint32_t* argb,
        std::size_t count,
        Asset* output) {
    if (output == nullptr || width <= 0 || height <= 0 || argb == nullptr ||
            count == 0 || count > kMaxPixels ||
            count != static_cast<std::size_t>(width) * static_cast<std::size_t>(height)) {
        return false;
    }
    Asset asset;
    asset.width = width;
    asset.height = height;
    asset.pixels.assign(argb, argb + count);
    if (!valid_asset(asset)) return false;
    *output = std::move(asset);
    return true;
}

bool current_surface(SurfaceMetrics* output) {
    if (output == nullptr) return false;
    const int width = g_surface_width.load(std::memory_order_relaxed);
    const int height = g_surface_height.load(std::memory_order_relaxed);
    if (width <= 0 || height <= 0) return false;
    const float scale = std::min(
        static_cast<float>(width) / kDesignWidth,
        static_cast<float>(height) / kDesignHeight);
    if (!std::isfinite(scale) || scale <= 0.0f) return false;
    output->width = width;
    output->height = height;
    output->scale = scale;
    output->offset_x = (static_cast<float>(width) - kDesignWidth * scale) * 0.5f;
    output->offset_y = (static_cast<float>(height) - kDesignHeight * scale) * 0.5f;
    return true;
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

void release_textures() {
    if (g_keyboard_texture != 0) glDeleteTextures(1, &g_keyboard_texture);
    if (g_continue_texture != 0) glDeleteTextures(1, &g_continue_texture);
    if (g_prompt_board_texture != 0) glDeleteTextures(1, &g_prompt_board_texture);
    if (g_prompt_titlebar_texture != 0) glDeleteTextures(1, &g_prompt_titlebar_texture);
    if (g_prompt_title_texture != 0) glDeleteTextures(1, &g_prompt_title_texture);
    if (g_prompt_body_texture != 0) glDeleteTextures(1, &g_prompt_body_texture);
    if (g_prompt_hints_texture != 0) glDeleteTextures(1, &g_prompt_hints_texture);
    g_keyboard_texture = 0;
    g_continue_texture = 0;
    g_prompt_board_texture = 0;
    g_prompt_titlebar_texture = 0;
    g_prompt_title_texture = 0;
    g_prompt_body_texture = 0;
    g_prompt_hints_texture = 0;
    g_texture_generation = 0;
    g_prompt_texture_generation = 0;
}

GLuint create_texture(const Asset& asset) {
    if (!valid_asset(asset)) return 0;
    std::vector<std::uint8_t> rgba(asset.pixels.size() * 4u);
    for (std::size_t index = 0; index < asset.pixels.size(); ++index) {
        const std::uint32_t pixel = asset.pixels[index];
        rgba[index * 4u] = static_cast<std::uint8_t>((pixel >> 16u) & 0xffu);
        rgba[index * 4u + 1u] = static_cast<std::uint8_t>((pixel >> 8u) & 0xffu);
        rgba[index * 4u + 2u] = static_cast<std::uint8_t>(pixel & 0xffu);
        rgba[index * 4u + 3u] = static_cast<std::uint8_t>((pixel >> 24u) & 0xffu);
    }

    GLuint texture = 0;
    glGenTextures(1, &texture);
    if (texture == 0) return 0;
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
        asset.width,
        asset.height,
        0,
        GL_RGBA,
        GL_UNSIGNED_BYTE,
        rgba.data());
    const GLenum error = glGetError();
    glBindTexture(GL_TEXTURE_2D, 0);
    if (error != GL_NO_ERROR) {
        glDeleteTextures(1, &texture);
        return 0;
    }
    return texture;
}

bool ensure_textures() {
    Asset keyboard;
    Asset continue_label;
    std::uint64_t generation = 0;
    {
        std::lock_guard<std::mutex> lock(g_mutex);
        if (!valid_asset(g_state.keyboard) || !valid_asset(g_state.continue_label)) return false;
        generation = g_state.asset_generation;
        if (g_texture_generation == generation &&
                g_keyboard_texture != 0 && g_continue_texture != 0) {
            return true;
        }
        keyboard = g_state.keyboard;
        continue_label = g_state.continue_label;
    }

    release_textures();
    const GLuint keyboard_texture = create_texture(keyboard);
    if (keyboard_texture == 0) return false;
    const GLuint continue_texture = create_texture(continue_label);
    if (continue_texture == 0) {
        glDeleteTextures(1, &keyboard_texture);
        return false;
    }
    g_keyboard_texture = keyboard_texture;
    g_continue_texture = continue_texture;
    g_texture_generation = generation;
    return true;
}

bool ensure_prompt_textures() {
    Asset board;
    Asset titlebar;
    Asset title;
    Asset body;
    Asset hints;
    std::uint64_t generation = 0;
    {
        std::lock_guard<std::mutex> lock(g_mutex);
        if (!valid_asset(g_state.prompt_board) || !valid_asset(g_state.prompt_titlebar) ||
                !valid_asset(g_state.prompt_title) || !valid_asset(g_state.prompt_body) ||
                !valid_asset(g_state.prompt_hints)) {
            return false;
        }
        generation = g_state.prompt_generation;
        if (g_prompt_texture_generation == generation &&
                g_prompt_board_texture != 0 && g_prompt_titlebar_texture != 0 &&
                g_prompt_title_texture != 0 && g_prompt_body_texture != 0 &&
                g_prompt_hints_texture != 0) {
            return true;
        }
        board = g_state.prompt_board;
        titlebar = g_state.prompt_titlebar;
        title = g_state.prompt_title;
        body = g_state.prompt_body;
        hints = g_state.prompt_hints;
    }

    if (g_prompt_board_texture != 0) glDeleteTextures(1, &g_prompt_board_texture);
    if (g_prompt_titlebar_texture != 0) glDeleteTextures(1, &g_prompt_titlebar_texture);
    if (g_prompt_title_texture != 0) glDeleteTextures(1, &g_prompt_title_texture);
    if (g_prompt_body_texture != 0) glDeleteTextures(1, &g_prompt_body_texture);
    if (g_prompt_hints_texture != 0) glDeleteTextures(1, &g_prompt_hints_texture);
    g_prompt_board_texture = 0;
    g_prompt_titlebar_texture = 0;
    g_prompt_title_texture = 0;
    g_prompt_body_texture = 0;
    g_prompt_hints_texture = 0;
    g_prompt_texture_generation = 0;

    const GLuint board_texture = create_texture(board);
    const GLuint titlebar_texture = create_texture(titlebar);
    const GLuint title_texture = create_texture(title);
    const GLuint body_texture = create_texture(body);
    const GLuint hints_texture = create_texture(hints);
    if (board_texture == 0 || titlebar_texture == 0 || title_texture == 0 ||
            body_texture == 0 || hints_texture == 0) {
        if (board_texture != 0) glDeleteTextures(1, &board_texture);
        if (titlebar_texture != 0) glDeleteTextures(1, &titlebar_texture);
        if (title_texture != 0) glDeleteTextures(1, &title_texture);
        if (body_texture != 0) glDeleteTextures(1, &body_texture);
        if (hints_texture != 0) glDeleteTextures(1, &hints_texture);
        return false;
    }
    g_prompt_board_texture = board_texture;
    g_prompt_titlebar_texture = titlebar_texture;
    g_prompt_title_texture = title_texture;
    g_prompt_body_texture = body_texture;
    g_prompt_hints_texture = hints_texture;
    g_prompt_texture_generation = generation;
    return true;
}

void draw_asset(
        GLuint texture,
        const Asset& asset,
        const SurfaceMetrics& surface,
        float design_center_x,
        float design_center_y) {
    if (texture == 0 || !valid_asset(asset)) return;
    const float center_x = surface.offset_x + design_center_x * surface.scale;
    const float center_y = surface.offset_y + (kDesignHeight - design_center_y) * surface.scale;
    const float half_width = static_cast<float>(asset.width) * surface.scale * 0.5f;
    const float half_height = static_cast<float>(asset.height) * surface.scale * 0.5f;
    const float left = center_x - half_width;
    const float right = center_x + half_width;
    const float top = center_y - half_height;
    const float bottom = center_y + half_height;

    const GLfloat vertices[] = {
        left * 2.0f / static_cast<float>(surface.width) - 1.0f,
        1.0f - top * 2.0f / static_cast<float>(surface.height),
        left * 2.0f / static_cast<float>(surface.width) - 1.0f,
        1.0f - bottom * 2.0f / static_cast<float>(surface.height),
        right * 2.0f / static_cast<float>(surface.width) - 1.0f,
        1.0f - top * 2.0f / static_cast<float>(surface.height),
        right * 2.0f / static_cast<float>(surface.width) - 1.0f,
        1.0f - bottom * 2.0f / static_cast<float>(surface.height),
    };
    static constexpr GLfloat kTexCoords[] = {
        0.0f, 0.0f,
        0.0f, 1.0f,
        1.0f, 0.0f,
        1.0f, 1.0f,
    };

    glBindTexture(GL_TEXTURE_2D, texture);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, vertices);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 0, kTexCoords);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
}

bool map_surface(float surface_x, float surface_y, float* design_x, float* design_y) {
    if (design_x == nullptr || design_y == nullptr ||
            !std::isfinite(surface_x) || !std::isfinite(surface_y)) {
        return false;
    }
    SurfaceMetrics surface;
    if (!current_surface(&surface)) return false;
    const float local_x = surface_x - surface.offset_x;
    const float local_y = surface_y - surface.offset_y;
    if (local_x < 0.0f || local_y < 0.0f ||
            local_x > kDesignWidth * surface.scale ||
            local_y > kDesignHeight * surface.scale) {
        return false;
    }
    *design_x = local_x / surface.scale;
    *design_y = kDesignHeight - local_y / surface.scale;
    return true;
}

bool continue_hit(float design_x, float design_y) {
    std::lock_guard<std::mutex> lock(g_mutex);
    if (!valid_asset(g_state.continue_label)) return false;
    const float half_width = static_cast<float>(g_state.continue_label.width) * 0.5f;
    const float half_height = static_cast<float>(g_state.continue_label.height) * 0.5f;
    return design_x >= kContinueCenterX - half_width &&
        design_x <= kContinueCenterX + half_width &&
        design_y >= kContinueCenterY - half_height &&
        design_y <= kContinueCenterY + half_height;
}

}  // namespace

void on_surface_created() {
    g_keyboard_texture = 0;
    g_continue_texture = 0;
    g_prompt_board_texture = 0;
    g_prompt_titlebar_texture = 0;
    g_prompt_title_texture = 0;
    g_prompt_body_texture = 0;
    g_prompt_hints_texture = 0;
    g_texture_generation = 0;
    g_prompt_texture_generation = 0;
    if (g_program != 0) glDeleteProgram(g_program);
    g_program = build_program();
    g_sampler = g_program != 0 ? glGetUniformLocation(g_program, "uTexture") : -1;
}

void on_surface_changed(int width, int height) {
    g_surface_width.store(width, std::memory_order_relaxed);
    g_surface_height.store(height, std::memory_order_relaxed);
}

void clear_assets() {
    std::lock_guard<std::mutex> lock(g_mutex);
    g_state.keyboard = {};
    g_state.continue_label = {};
    g_state.prompt_board = {};
    g_state.prompt_titlebar = {};
    g_state.prompt_title = {};
    g_state.prompt_body = {};
    g_state.prompt_hints = {};
    g_state.prompt_step = -1;
    g_state.captured_pointer = -1;
    ++g_state.asset_generation;
    ++g_state.prompt_generation;
}

bool upload_keyboard(
        int width,
        int height,
        const std::uint32_t* argb,
        std::size_t count) {
    Asset asset;
    if (!make_asset(width, height, argb, count, &asset)) return false;
    std::lock_guard<std::mutex> lock(g_mutex);
    g_state.keyboard = std::move(asset);
    ++g_state.asset_generation;
    return true;
}

bool upload_continue_label(
        int width,
        int height,
        const std::uint32_t* argb,
        std::size_t count) {
    Asset asset;
    if (!make_asset(width, height, argb, count, &asset)) return false;
    std::lock_guard<std::mutex> lock(g_mutex);
    g_state.continue_label = std::move(asset);
    ++g_state.asset_generation;
    return true;
}

bool upload_prompt_backgrounds(
        int board_width,
        int board_height,
        const std::uint32_t* board_argb,
        std::size_t board_count,
        int titlebar_width,
        int titlebar_height,
        const std::uint32_t* titlebar_argb,
        std::size_t titlebar_count) {
    Asset board;
    Asset titlebar;
    if (!make_asset(board_width, board_height, board_argb, board_count, &board) ||
            !make_asset(titlebar_width, titlebar_height, titlebar_argb, titlebar_count, &titlebar)) {
        return false;
    }
    std::lock_guard<std::mutex> lock(g_mutex);
    g_state.prompt_board = std::move(board);
    g_state.prompt_titlebar = std::move(titlebar);
    ++g_state.prompt_generation;
    return true;
}

bool upload_prompt_text(
        int tutorial_step,
        int title_width,
        int title_height,
        const std::uint32_t* title_argb,
        std::size_t title_count,
        int body_width,
        int body_height,
        const std::uint32_t* body_argb,
        std::size_t body_count) {
    if (tutorial_step < 2 || tutorial_step > 10) return false;
    Asset title;
    Asset body;
    if (!make_asset(title_width, title_height, title_argb, title_count, &title) ||
            !make_asset(body_width, body_height, body_argb, body_count, &body)) {
        return false;
    }
    std::lock_guard<std::mutex> lock(g_mutex);
    g_state.prompt_title = std::move(title);
    g_state.prompt_body = std::move(body);
    g_state.prompt_step = tutorial_step;
    ++g_state.prompt_generation;
    return true;
}

bool upload_prompt_hints(
        int tutorial_step,
        int width,
        int height,
        const std::uint32_t* argb,
        std::size_t count) {
    if (tutorial_step < 2 || tutorial_step > 10) return false;
    Asset hints;
    if (!make_asset(width, height, argb, count, &hints)) return false;
    std::lock_guard<std::mutex> lock(g_mutex);
    g_state.prompt_hints = std::move(hints);
    g_state.prompt_step = tutorial_step;
    ++g_state.prompt_generation;
    return true;
}

void clear_prompt() {
    std::lock_guard<std::mutex> lock(g_mutex);
    g_state.prompt_board = {};
    g_state.prompt_titlebar = {};
    g_state.prompt_title = {};
    g_state.prompt_body = {};
    g_state.prompt_hints = {};
    g_state.prompt_step = -1;
    ++g_state.prompt_generation;
}

bool draw() {
    const auto phase = tutorial_session_state::snapshot().phase;
    const bool modal = phase == tutorial_session_state::Phase::kLessonOverlay;
    const bool interactive = phase == tutorial_session_state::Phase::kInteractive;
    if (!modal && !interactive) return false;
    {
        std::lock_guard<std::mutex> lock(g_mutex);
        ++g_state.draw_attempt_count;
    }
    SurfaceMetrics surface;
    const bool textures_ready = modal ? ensure_textures() : ensure_prompt_textures();
    if (g_program == 0 || g_sampler < 0 || !current_surface(&surface) || !textures_ready) {
        return false;
    }

    Asset keyboard;
    Asset continue_label;
    Asset prompt_board;
    Asset prompt_titlebar;
    Asset prompt_title;
    Asset prompt_body;
    Asset prompt_hints;
    int prompt_step = -1;
    {
        std::lock_guard<std::mutex> lock(g_mutex);
        if (modal) {
            keyboard = g_state.keyboard;
            continue_label = g_state.continue_label;
        } else {
            prompt_board = g_state.prompt_board;
            prompt_titlebar = g_state.prompt_titlebar;
            prompt_title = g_state.prompt_title;
            prompt_body = g_state.prompt_body;
            prompt_hints = g_state.prompt_hints;
            prompt_step = g_state.prompt_step;
        }
    }
    if (interactive && prompt_step !=
            static_cast<int>(tutorial_gameplay_state::snapshot().step)) {
        return false;
    }

    while (glGetError() != GL_NO_ERROR) {
    }
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glUseProgram(g_program);
    glActiveTexture(GL_TEXTURE0);
    glUniform1i(g_sampler, 0);
    glEnableVertexAttribArray(0);
    glEnableVertexAttribArray(1);
    if (modal) {
        draw_asset(
            g_keyboard_texture,
            keyboard,
            surface,
            kDesignWidth * 0.5f,
            kDesignHeight * 0.5f);
        draw_asset(g_continue_texture, continue_label, surface, kContinueCenterX, kContinueCenterY);
    } else {
        draw_asset(g_prompt_board_texture, prompt_board, surface, kPromptCenterX, kPromptCenterY);
        draw_asset(
            g_prompt_titlebar_texture,
            prompt_titlebar,
            surface,
            kPromptCenterX,
            kPromptCenterY);
        draw_asset(
            g_prompt_title_texture,
            prompt_title,
            surface,
            kPromptCenterX - 115.0f,
            kPromptCenterY);
        draw_asset(
            g_prompt_body_texture,
            prompt_body,
            surface,
            kPromptCenterX,
            kPromptCenterY - 60.0f);
        draw_asset(
            g_prompt_hints_texture,
            prompt_hints,
            surface,
            kDesignWidth * 0.5f,
            kDesignHeight * 0.5f);
    }
    glDisableVertexAttribArray(1);
    glDisableVertexAttribArray(0);
    glBindTexture(GL_TEXTURE_2D, 0);
    glDisable(GL_BLEND);

    if (glGetError() != GL_NO_ERROR) return false;
    std::lock_guard<std::mutex> lock(g_mutex);
    ++g_state.drawn_frame_count;
    return true;
}

bool on_touch(int action, int pointer_id, float surface_x, float surface_y) {
    if (tutorial_session_state::snapshot().phase !=
            tutorial_session_state::Phase::kLessonOverlay) {
        return false;
    }

    float design_x = 0.0f;
    float design_y = 0.0f;
    const bool mapped = map_surface(surface_x, surface_y, &design_x, &design_y);

    if (action == kActionCancel) {
        std::lock_guard<std::mutex> lock(g_mutex);
        if (g_state.captured_pointer >= 0) ++g_state.cancel_count;
        g_state.captured_pointer = -1;
        return true;
    }

    if (action == kActionDown || action == kActionPointerDown) {
        if (mapped && continue_hit(design_x, design_y)) {
            std::lock_guard<std::mutex> lock(g_mutex);
            if (g_state.captured_pointer < 0) {
                g_state.captured_pointer = pointer_id;
                ++g_state.accepted_begin_count;
            }
        }
        return true;
    }

    if (action == kActionMove) return true;

    if (action == kActionUp || action == kActionPointerUp) {
        bool captured = false;
        {
            std::lock_guard<std::mutex> lock(g_mutex);
            captured = g_state.captured_pointer == pointer_id;
            if (captured) {
                g_state.captured_pointer = -1;
                ++g_state.accepted_end_count;
            }
        }
        if (!captured) return true;
        const bool tapped = mapped && continue_hit(design_x, design_y);
        if (!tapped) return true;
        {
            std::lock_guard<std::mutex> lock(g_mutex);
            ++g_state.continue_tap_count;
        }
        if (tutorial_session_state::on_lesson_overlay_complete()) {
            std::lock_guard<std::mutex> lock(g_mutex);
            ++g_state.completion_count;
        }
        return true;
    }

    return true;
}

std::string status_report() {
    std::lock_guard<std::mutex> lock(g_mutex);
    std::ostringstream out;
    out << "Tutorial LessonOverlay\n";
    out << "assets ready: "
        << (valid_asset(g_state.keyboard) && valid_asset(g_state.continue_label) ? "yes" : "no")
        << "\n";
    out << "asset generation: " << g_state.asset_generation << "\n";
    out << "interactive prompt ready: "
        << (valid_asset(g_state.prompt_board) && valid_asset(g_state.prompt_titlebar) &&
            valid_asset(g_state.prompt_title) && valid_asset(g_state.prompt_body) &&
            valid_asset(g_state.prompt_hints) ? "yes" : "no")
        << "\n";
    out << "interactive prompt step: " << g_state.prompt_step << "\n";
    out << "interactive prompt generation: " << g_state.prompt_generation << "\n";
    if (valid_asset(g_state.keyboard)) {
        out << "keyboard pixels: " << g_state.keyboard.width << "x" << g_state.keyboard.height << "\n";
    }
    if (valid_asset(g_state.continue_label)) {
        out << "continue label pixels: " << g_state.continue_label.width << "x"
            << g_state.continue_label.height << "\n";
    }
    out << "draw attempts: " << g_state.draw_attempt_count << "\n";
    out << "drawn frames: " << g_state.drawn_frame_count << "\n";
    out << "continue begins: " << g_state.accepted_begin_count << "\n";
    out << "continue ends: " << g_state.accepted_end_count << "\n";
    out << "continue taps: " << g_state.continue_tap_count << "\n";
    out << "overlay completions: " << g_state.completion_count << "\n";
    out << "cancelled captures: " << g_state.cancel_count << "\n";
    return out.str();
}

}  // namespace nevergone::tutorial_lesson_overlay

namespace {

std::string jstring_to_utf8(JNIEnv* env, jstring value) {
    if (env == nullptr || value == nullptr) return {};
    const char* chars = env->GetStringUTFChars(value, nullptr);
    if (chars == nullptr) return {};
    std::string result(chars);
    env->ReleaseStringUTFChars(value, chars);
    return result;
}

bool upload_java_asset(
        JNIEnv* env,
        jint width,
        jint height,
        jintArray pixels,
        bool keyboard) {
    if (env == nullptr || pixels == nullptr || width <= 0 || height <= 0) return false;
    const jsize length = env->GetArrayLength(pixels);
    const std::size_t expected =
        static_cast<std::size_t>(width) * static_cast<std::size_t>(height);
    if (static_cast<std::size_t>(length) != expected || expected > 16u * 1024u * 1024u) {
        return false;
    }
    jint* values = env->GetIntArrayElements(pixels, nullptr);
    if (values == nullptr) return false;
    const bool ok = keyboard
        ? nevergone::tutorial_lesson_overlay::upload_keyboard(
            static_cast<int>(width),
            static_cast<int>(height),
            reinterpret_cast<const std::uint32_t*>(values),
            expected)
        : nevergone::tutorial_lesson_overlay::upload_continue_label(
            static_cast<int>(width),
            static_cast<int>(height),
            reinterpret_cast<const std::uint32_t*>(values),
            expected);
    env->ReleaseIntArrayElements(pixels, values, JNI_ABORT);
    return ok;
}

bool copy_java_pixels(
        JNIEnv* env,
        jint width,
        jint height,
        jintArray pixels,
        std::vector<std::uint32_t>* output) {
    if (env == nullptr || pixels == nullptr || output == nullptr || width <= 0 || height <= 0) {
        return false;
    }
    const jsize length = env->GetArrayLength(pixels);
    const std::size_t expected =
        static_cast<std::size_t>(width) * static_cast<std::size_t>(height);
    if (static_cast<std::size_t>(length) != expected || expected > 16u * 1024u * 1024u) {
        return false;
    }
    output->resize(expected);
    env->GetIntArrayRegion(
        pixels,
        0,
        length,
        reinterpret_cast<jint*>(output->data()));
    if (env->ExceptionCheck()) {
        env->ExceptionClear();
        output->clear();
        return false;
    }
    return true;
}

bool upload_java_prompt_backgrounds(
        JNIEnv* env,
        jint board_width,
        jint board_height,
        jintArray board_pixels,
        jint titlebar_width,
        jint titlebar_height,
        jintArray titlebar_pixels) {
    std::vector<std::uint32_t> board;
    std::vector<std::uint32_t> titlebar;
    if (!copy_java_pixels(env, board_width, board_height, board_pixels, &board) ||
            !copy_java_pixels(env, titlebar_width, titlebar_height, titlebar_pixels, &titlebar)) {
        return false;
    }
    return nevergone::tutorial_lesson_overlay::upload_prompt_backgrounds(
        board_width,
        board_height,
        board.data(),
        board.size(),
        titlebar_width,
        titlebar_height,
        titlebar.data(),
        titlebar.size());
}

bool upload_java_prompt_text(
        JNIEnv* env,
        jint step,
        jint title_width,
        jint title_height,
        jintArray title_pixels,
        jint body_width,
        jint body_height,
        jintArray body_pixels) {
    std::vector<std::uint32_t> title;
    std::vector<std::uint32_t> body;
    if (!copy_java_pixels(env, title_width, title_height, title_pixels, &title) ||
            !copy_java_pixels(env, body_width, body_height, body_pixels, &body)) {
        return false;
    }
    return nevergone::tutorial_lesson_overlay::upload_prompt_text(
        step,
        title_width,
        title_height,
        title.data(),
        title.size(),
        body_width,
        body_height,
        body.data(),
        body.size());
}

bool upload_java_prompt_hints(
        JNIEnv* env,
        jint step,
        jint width,
        jint height,
        jintArray pixels) {
    std::vector<std::uint32_t> hints;
    if (!copy_java_pixels(env, width, height, pixels, &hints)) return false;
    return nevergone::tutorial_lesson_overlay::upload_prompt_hints(
        step, width, height, hints.data(), hints.size());
}

}  // namespace

extern "C" JNIEXPORT void JNICALL
Java_org_nevergone_recomp_TutorialLessonOverlayAssetLoader_nativeClear(JNIEnv*, jclass) {
    nevergone::tutorial_lesson_overlay::clear_assets();
}

extern "C" JNIEXPORT jboolean JNICALL
Java_org_nevergone_recomp_TutorialLessonOverlayAssetLoader_nativeUploadKeyboard(
        JNIEnv* env,
        jclass,
        jint width,
        jint height,
        jintArray pixels) {
    return upload_java_asset(env, width, height, pixels, true) ? JNI_TRUE : JNI_FALSE;
}

extern "C" JNIEXPORT jboolean JNICALL
Java_org_nevergone_recomp_TutorialLessonOverlayAssetLoader_nativeUploadContinueLabel(
        JNIEnv* env,
        jclass,
        jint width,
        jint height,
        jintArray pixels) {
    return upload_java_asset(env, width, height, pixels, false) ? JNI_TRUE : JNI_FALSE;
}

extern "C" JNIEXPORT void JNICALL
Java_org_nevergone_recomp_TutorialLessonOverlayAssetLoader_nativeClearPrompt(JNIEnv*, jclass) {
    nevergone::tutorial_lesson_overlay::clear_prompt();
}

extern "C" JNIEXPORT jboolean JNICALL
Java_org_nevergone_recomp_TutorialLessonOverlayAssetLoader_nativeUploadPromptBackgrounds(
        JNIEnv* env,
        jclass,
        jint board_width,
        jint board_height,
        jintArray board_pixels,
        jint titlebar_width,
        jint titlebar_height,
        jintArray titlebar_pixels) {
    return upload_java_prompt_backgrounds(
        env,
        board_width,
        board_height,
        board_pixels,
        titlebar_width,
        titlebar_height,
        titlebar_pixels) ? JNI_TRUE : JNI_FALSE;
}

extern "C" JNIEXPORT jboolean JNICALL
Java_org_nevergone_recomp_TutorialLessonOverlayAssetLoader_nativeUploadPromptText(
        JNIEnv* env,
        jclass,
        jint step,
        jint title_width,
        jint title_height,
        jintArray title_pixels,
        jint body_width,
        jint body_height,
        jintArray body_pixels) {
    return upload_java_prompt_text(
        env,
        step,
        title_width,
        title_height,
        title_pixels,
        body_width,
        body_height,
        body_pixels) ? JNI_TRUE : JNI_FALSE;
}

extern "C" JNIEXPORT jboolean JNICALL
Java_org_nevergone_recomp_TutorialLessonOverlayAssetLoader_nativeUploadPromptHints(
        JNIEnv* env,
        jclass,
        jint step,
        jint width,
        jint height,
        jintArray pixels) {
    return upload_java_prompt_hints(env, step, width, height, pixels) ? JNI_TRUE : JNI_FALSE;
}

extern "C" JNIEXPORT jstring JNICALL
Java_org_nevergone_recomp_TutorialLessonOverlayAssetLoader_nativeLocalizedString(
        JNIEnv* env,
        jclass,
        jstring files_dir,
        jstring key,
        jint language_column) {
    if (env == nullptr || files_dir == nullptr || key == nullptr || language_column < 0) {
        return nullptr;
    }
    std::string value;
    if (!nevergone::login_localization_csv::resolve_key_file(
            jstring_to_utf8(env, files_dir),
            jstring_to_utf8(env, key),
            static_cast<std::size_t>(language_column),
            &value) || value.empty()) {
        return nullptr;
    }
    return env->NewStringUTF(value.c_str());
}
