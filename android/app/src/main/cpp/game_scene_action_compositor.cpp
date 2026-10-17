#include "game_scene_action_compositor.h"

#include <GLES2/gl2.h>

#include <array>
#include <mutex>
#include <sstream>
#include <vector>

#include "game_scene_action_layout.h"
#include "game_scene_action_surface_input.h"
#include "offline_startup_flow.h"

namespace nevergone::game_scene_action_compositor {
namespace {

constexpr float kDesignWidth = 1136.0f;
constexpr float kDesignHeight = 640.0f;

struct SpriteTexture {
    GLuint id = 0;
    int width = 0;
    int height = 0;
};

std::mutex g_mutex;
std::array<std::array<SpriteTexture, 2>, game_scene_action_layout::kButtonCount> g_textures{};
GLuint g_program = 0;
GLint g_sampler = -1;
int g_surface_width = 0;
int g_surface_height = 0;
std::uint64_t g_upload_count = 0;
std::uint64_t g_draw_count = 0;

GLuint compile(GLenum type, const char* source) {
    const GLuint shader = glCreateShader(type);
    if (shader == 0) return 0;
    glShaderSource(shader, 1, &source, nullptr);
    glCompileShader(shader);
    GLint ok = GL_FALSE;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
    if (ok == GL_TRUE) return shader;
    glDeleteShader(shader);
    return 0;
}

GLuint build_program() {
    static constexpr const char* kVertex = R"(
attribute vec2 aPosition;
attribute vec2 aTexCoord;
varying vec2 vTexCoord;
void main() {
    vTexCoord = aTexCoord;
    gl_Position = vec4(aPosition, 0.0, 1.0);
})";
    static constexpr const char* kFragment = R"(
precision mediump float;
varying vec2 vTexCoord;
uniform sampler2D uTexture;
void main() {
    gl_FragColor = texture2D(uTexture, vTexCoord);
})";
    const GLuint vs = compile(GL_VERTEX_SHADER, kVertex);
    const GLuint fs = compile(GL_FRAGMENT_SHADER, kFragment);
    if (vs == 0 || fs == 0) {
        if (vs != 0) glDeleteShader(vs);
        if (fs != 0) glDeleteShader(fs);
        return 0;
    }
    const GLuint program = glCreateProgram();
    if (program == 0) {
        glDeleteShader(vs);
        glDeleteShader(fs);
        return 0;
    }
    glAttachShader(program, vs);
    glAttachShader(program, fs);
    glBindAttribLocation(program, 0, "aPosition");
    glBindAttribLocation(program, 1, "aTexCoord");
    glLinkProgram(program);
    glDeleteShader(vs);
    glDeleteShader(fs);
    GLint ok = GL_FALSE;
    glGetProgramiv(program, GL_LINK_STATUS, &ok);
    if (ok != GL_TRUE) {
        glDeleteProgram(program);
        return 0;
    }
    return program;
}

void destroy_texture(SpriteTexture* texture) {
    if (texture == nullptr) return;
    if (texture->id != 0) glDeleteTextures(1, &texture->id);
    *texture = {};
}

void destroy_all_locked() {
    for (auto& pair : g_textures) {
        destroy_texture(&pair[0]);
        destroy_texture(&pair[1]);
    }
}

bool control_pressed(
        const game_scene_action_surface_input::Snapshot& input,
        int slot) {
    if (slot < 0 || slot >= game_scene_action_layout::kButtonCount) return false;
    return input.pressed[static_cast<std::size_t>(slot)];
}

}  // namespace

void on_surface_created() {
    std::lock_guard<std::mutex> lock(g_mutex);
    destroy_all_locked();
    if (g_program != 0) glDeleteProgram(g_program);
    g_program = build_program();
    g_sampler = g_program != 0 ? glGetUniformLocation(g_program, "uTexture") : -1;
    g_upload_count = 0;
    g_draw_count = 0;
}

void on_surface_changed(int width, int height) {
    std::lock_guard<std::mutex> lock(g_mutex);
    g_surface_width = width;
    g_surface_height = height;
}

bool upload_sprite(
        int slot,
        bool selected,
        int width,
        int height,
        const std::uint32_t* argb_pixels,
        std::size_t pixel_count) {
    if (slot < 0 || slot >= game_scene_action_layout::kButtonCount ||
        width <= 0 || height <= 0 || width > 4096 || height > 4096 ||
        argb_pixels == nullptr ||
        pixel_count != static_cast<std::size_t>(width) * static_cast<std::size_t>(height) ||
        pixel_count > 16777216u) {
        return false;
    }

    std::vector<std::uint8_t> rgba(pixel_count * 4u);
    for (std::size_t i = 0; i < pixel_count; ++i) {
        const std::uint32_t p = argb_pixels[i];
        rgba[i * 4u + 0u] = static_cast<std::uint8_t>((p >> 16u) & 0xffu);
        rgba[i * 4u + 1u] = static_cast<std::uint8_t>((p >> 8u) & 0xffu);
        rgba[i * 4u + 2u] = static_cast<std::uint8_t>(p & 0xffu);
        rgba[i * 4u + 3u] = static_cast<std::uint8_t>((p >> 24u) & 0xffu);
    }

    GLuint id = 0;
    glGenTextures(1, &id);
    if (id == 0) return false;
    glBindTexture(GL_TEXTURE_2D, id);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexImage2D(
        GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0,
        GL_RGBA, GL_UNSIGNED_BYTE, rgba.data());
    const GLenum error = glGetError();
    glBindTexture(GL_TEXTURE_2D, 0);
    if (error != GL_NO_ERROR) {
        glDeleteTextures(1, &id);
        return false;
    }

    std::lock_guard<std::mutex> lock(g_mutex);
    SpriteTexture& target = g_textures[static_cast<std::size_t>(slot)][selected ? 1u : 0u];
    destroy_texture(&target);
    target.id = id;
    target.width = width;
    target.height = height;
    ++g_upload_count;
    return true;
}

void clear_sprites() {
    std::lock_guard<std::mutex> lock(g_mutex);
    destroy_all_locked();
}

bool draw() {
    if (offline_startup_flow::snapshot().route != offline_startup_flow::Route::kGameLevels) {
        return false;
    }
    const auto input = game_scene_action_surface_input::snapshot();

    std::lock_guard<std::mutex> lock(g_mutex);
    if (g_program == 0 || g_sampler < 0 || g_surface_width <= 0 || g_surface_height <= 0) {
        return false;
    }

    static constexpr GLfloat kTexCoords[] = {
        0.0f, 1.0f,
        0.0f, 0.0f,
        1.0f, 1.0f,
        1.0f, 0.0f,
    };

    bool drew = false;
    glUseProgram(g_program);
    glActiveTexture(GL_TEXTURE0);
    glUniform1i(g_sampler, 0);
    glEnableVertexAttribArray(0);
    glEnableVertexAttribArray(1);

    for (int slot = 0; slot < game_scene_action_layout::kButtonCount; ++slot) {
        const bool pressed = control_pressed(input, slot);
        const SpriteTexture& selected = g_textures[static_cast<std::size_t>(slot)][1];
        const SpriteTexture& normal = g_textures[static_cast<std::size_t>(slot)][0];
        const SpriteTexture& texture = pressed && selected.id != 0 ? selected : normal;
        if (texture.id == 0 || texture.width <= 0 || texture.height <= 0) continue;

        const auto center = input.positions.buttons[static_cast<std::size_t>(slot)];
        const float center_x = center.x;
        const float center_y = center.y;
        const float half_w = static_cast<float>(texture.width) * 0.5f;
        const float half_h = static_cast<float>(texture.height) * 0.5f;
        const float left = center_x - half_w;
        const float right = center_x + half_w;
        const float bottom = center_y - half_h;
        const float top = center_y + half_h;
        const GLfloat vertices[] = {
            left / kDesignWidth * 2.0f - 1.0f,
            bottom / kDesignHeight * 2.0f - 1.0f,
            left / kDesignWidth * 2.0f - 1.0f,
            top / kDesignHeight * 2.0f - 1.0f,
            right / kDesignWidth * 2.0f - 1.0f,
            bottom / kDesignHeight * 2.0f - 1.0f,
            right / kDesignWidth * 2.0f - 1.0f,
            top / kDesignHeight * 2.0f - 1.0f,
        };

        glBindTexture(GL_TEXTURE_2D, texture.id);
        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, vertices);
        glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 0, kTexCoords);
        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
        drew = true;
    }

    glDisableVertexAttribArray(1);
    glDisableVertexAttribArray(0);
    glBindTexture(GL_TEXTURE_2D, 0);
    if (drew) ++g_draw_count;
    return drew;
}

std::string status_report() {
    std::lock_guard<std::mutex> lock(g_mutex);
    std::ostringstream out;
    out << "GameScene action compositor\n";
    out << "program: " << (g_program != 0 ? "ready" : "unavailable") << "\n";
    out << "surface: " << g_surface_width << "x" << g_surface_height << "\n";
    out << "uploads: " << g_upload_count << "\n";
    out << "draws: " << g_draw_count << "\n";
    return out.str();
}

}  // namespace nevergone::game_scene_action_compositor
