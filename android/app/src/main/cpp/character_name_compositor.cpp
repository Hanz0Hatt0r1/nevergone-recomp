#include "character_name_compositor.h"

#include <GLES2/gl2.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

#include "character_name_assets.h"
#include "character_name_layout.h"
#include "character_name_state.h"

namespace nevergone::character_name_compositor {
namespace {

constexpr float kDesignWidth = 1136.0f;
constexpr float kDesignHeight = 640.0f;
constexpr float kFixedButtonLabelMaxWidth = 115.0f;

GLuint g_program = 0;
GLint g_sampler = -1;
std::array<GLuint, character_name_assets::kImageCount> g_image_textures{};
std::array<GLuint, character_name_assets::kLabelCount> g_label_textures{};
character_name_assets::Snapshot g_assets;
std::uint64_t g_asset_generation = static_cast<std::uint64_t>(-1);

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
    if (linked == GL_TRUE) return program;
    glDeleteProgram(program);
    return 0;
}

void delete_textures() {
    for (GLuint& texture : g_image_textures) {
        if (texture != 0) glDeleteTextures(1, &texture);
        texture = 0;
    }
    for (GLuint& texture : g_label_textures) {
        if (texture != 0) glDeleteTextures(1, &texture);
        texture = 0;
    }
}

GLuint create_texture(const character_name_assets::Asset& asset) {
    if (asset.width <= 0 || asset.height <= 0 ||
            asset.pixels.size() !=
                static_cast<std::size_t>(asset.width) * static_cast<std::size_t>(asset.height)) {
        return 0;
    }

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

void sync_assets() {
    const std::uint64_t generation = character_name_assets::generation();
    if (generation == g_asset_generation) return;
    delete_textures();
    g_assets = character_name_assets::snapshot();
    g_asset_generation = generation;
}

GLuint image_texture(int slot) {
    if (slot < 0 || slot >= character_name_assets::kImageCount) return 0;
    GLuint& texture = g_image_textures[static_cast<std::size_t>(slot)];
    if (texture == 0) {
        texture = create_texture(g_assets.images[static_cast<std::size_t>(slot)]);
    }
    return texture;
}

GLuint label_texture(int slot) {
    if (slot < 0 || slot >= character_name_assets::kLabelCount) return 0;
    GLuint& texture = g_label_textures[static_cast<std::size_t>(slot)];
    if (texture == 0) {
        texture = create_texture(g_assets.labels[static_cast<std::size_t>(slot)]);
    }
    return texture;
}

struct SurfaceTransform {
    float half_width = 1.0f;
    float half_height = 1.0f;
};

SurfaceTransform surface_transform(int surface_width, int surface_height) {
    SurfaceTransform transform;
    const float design_aspect = kDesignWidth / kDesignHeight;
    const float surface_aspect =
        static_cast<float>(surface_width) / static_cast<float>(surface_height);
    if (design_aspect > surface_aspect) {
        transform.half_height = surface_aspect / design_aspect;
    } else {
        transform.half_width = design_aspect / surface_aspect;
    }
    return transform;
}

float map_x(float x, const SurfaceTransform& transform) {
    return -transform.half_width +
        2.0f * transform.half_width * x / kDesignWidth;
}

float map_y(float y, const SurfaceTransform& transform) {
    return -transform.half_height +
        2.0f * transform.half_height * y / kDesignHeight;
}

void draw_texture(
        GLuint texture,
        float center_x,
        float center_y,
        float width,
        float height,
        const SurfaceTransform& transform) {
    if (texture == 0 || width <= 0.0f || height <= 0.0f) return;
    const float x0 = map_x(center_x - width * 0.5f, transform);
    const float x1 = map_x(center_x + width * 0.5f, transform);
    const float y0 = map_y(center_y - height * 0.5f, transform);
    const float y1 = map_y(center_y + height * 0.5f, transform);
    const GLfloat vertices[] = {
        x0, y0,
        x0, y1,
        x1, y0,
        x1, y1,
    };
    static constexpr GLfloat kTexCoords[] = {
        0.0f, 1.0f,
        0.0f, 0.0f,
        1.0f, 1.0f,
        1.0f, 0.0f,
    };

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glUseProgram(g_program);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, texture);
    glUniform1i(g_sampler, 0);
    glEnableVertexAttribArray(0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, vertices);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 0, kTexCoords);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    glDisableVertexAttribArray(1);
    glDisableVertexAttribArray(0);
    glBindTexture(GL_TEXTURE_2D, 0);
    glDisable(GL_BLEND);
}

void draw_asset(
        int slot,
        const character_name_layout::Pose& pose,
        const SurfaceTransform& transform) {
    const auto& asset = g_assets.images[static_cast<std::size_t>(slot)];
    draw_texture(
        image_texture(slot),
        pose.center_x,
        pose.center_y,
        static_cast<float>(asset.width),
        static_cast<float>(asset.height),
        transform);
}

void draw_label(
        int slot,
        float center_x,
        float center_y,
        bool fixed_button_label,
        const SurfaceTransform& transform) {
    const auto& asset = g_assets.labels[static_cast<std::size_t>(slot)];
    float width = static_cast<float>(asset.width);
    float height = static_cast<float>(asset.height);
    if (fixed_button_label && width > kFixedButtonLabelMaxWidth) {
        const float scale = kFixedButtonLabelMaxWidth / width;
        width *= scale;
        height *= scale;
    }
    draw_texture(label_texture(slot), center_x, center_y, width, height, transform);
}

}  // namespace

void on_surface_created() {
    // Texture names from a previous GL context are invalid. CPU asset backing
    // is retained in character_name_assets and will be re-uploaded lazily.
    for (GLuint& texture : g_image_textures) texture = 0;
    for (GLuint& texture : g_label_textures) texture = 0;
    if (g_program != 0) glDeleteProgram(g_program);
    g_program = build_program();
    g_sampler = g_program != 0 ? glGetUniformLocation(g_program, "uTexture") : -1;
    g_asset_generation = static_cast<std::uint64_t>(-1);
}

void draw(int surface_width, int surface_height) {
    if (!character_name_state::snapshot().active || surface_width <= 0 || surface_height <= 0) {
        return;
    }
    if (g_program == 0 || g_sampler < 0) return;

    sync_assets();
    if (!g_assets.ready) return;

    const auto& background = g_assets.images[
        static_cast<std::size_t>(character_name_assets::ImageSlot::kBackground)];
    const auto& name_plate = g_assets.images[
        static_cast<std::size_t>(character_name_assets::ImageSlot::kNamePlate)];
    const auto& random_button = g_assets.images[
        static_cast<std::size_t>(character_name_assets::ImageSlot::kRandomButton)];
    const auto& fixed_button = g_assets.images[
        static_cast<std::size_t>(character_name_assets::ImageSlot::kFixedButtonNormal)];

    const character_name_layout::Layout layout = character_name_layout::compute(
        kDesignWidth,
        kDesignHeight,
        static_cast<float>(background.width),
        static_cast<float>(background.height),
        static_cast<float>(name_plate.width),
        static_cast<float>(name_plate.height),
        static_cast<float>(random_button.width),
        static_cast<float>(random_button.height),
        static_cast<float>(fixed_button.width),
        static_cast<float>(fixed_button.height),
        static_cast<float>(fixed_button.width),
        static_cast<float>(fixed_button.height));
    if (!layout.valid) return;

    const SurfaceTransform transform = surface_transform(surface_width, surface_height);
    draw_asset(
        static_cast<int>(character_name_assets::ImageSlot::kBackground),
        layout.background,
        transform);
    draw_label(
        static_cast<int>(character_name_assets::LabelSlot::kTitle),
        layout.title.center_x,
        layout.title.center_y,
        false,
        transform);
    draw_asset(
        static_cast<int>(character_name_assets::ImageSlot::kNamePlate),
        layout.name_plate,
        transform);
    draw_asset(
        static_cast<int>(character_name_assets::ImageSlot::kRandomButton),
        layout.random_button,
        transform);
    draw_asset(
        static_cast<int>(character_name_assets::ImageSlot::kFixedButtonNormal),
        layout.confirm_button,
        transform);
    draw_label(
        static_cast<int>(character_name_assets::LabelSlot::kConfirm),
        layout.confirm_button.center_x,
        layout.confirm_button.center_y,
        true,
        transform);
    draw_asset(
        static_cast<int>(character_name_assets::ImageSlot::kFixedButtonNormal),
        layout.cancel_button,
        transform);
    draw_label(
        static_cast<int>(character_name_assets::LabelSlot::kCancel),
        layout.cancel_button.center_x,
        layout.cancel_button.center_y,
        true,
        transform);
}

}  // namespace nevergone::character_name_compositor
