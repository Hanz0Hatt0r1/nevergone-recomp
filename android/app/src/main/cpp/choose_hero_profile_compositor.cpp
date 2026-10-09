#include "choose_hero_profile_compositor.h"

#include <GLES2/gl2.h>

#include <array>
#include <atomic>
#include <cstdint>
#include <sstream>
#include <vector>

#include "choose_hero_profile_assets.h"
#include "choose_hero_role_item_assets.h"
#include "choose_hero_role_item_view.h"
#include "choose_hero_role_selection_state.h"
#include "offline_startup_flow.h"

namespace nevergone::choose_hero_profile_compositor {
namespace {

constexpr std::size_t kTextureCount = 1u + 2u * choose_hero_profile_assets::kLabelCount;
constexpr float kTextTint = 96.0f / 255.0f;

struct TextureAsset {
    GLuint texture = 0;
    choose_hero_profile_assets::Asset asset;
};

GLuint g_program = 0;
GLint g_sampler = -1;
GLint g_tint = -1;
std::array<TextureAsset, kTextureCount> g_textures{};
std::uint64_t g_texture_generation = 0;
std::atomic<std::uint64_t> g_draw_count{0};
std::atomic<std::uint64_t> g_profile_draw_count{0};

bool route_active() {
    return offline_startup_flow::snapshot().route == offline_startup_flow::Route::kChooseRole;
}

std::size_t label_texture_index(std::uint32_t slot_id, int kind) {
    return 1u + static_cast<std::size_t>(slot_id - 1u) *
        static_cast<std::size_t>(choose_hero_profile_assets::kLabelCount) +
        static_cast<std::size_t>(kind);
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
    g_sampler = -1;
    g_tint = -1;

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
uniform float uTint;
void main() {
    vec4 color = texture2D(uTexture, vTexCoord);
    gl_FragColor = vec4(color.rgb * uTint, color.a);
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
    g_tint = glGetUniformLocation(program, "uTint");
    if (g_sampler < 0 || g_tint < 0) {
        glDeleteProgram(program);
        g_program = 0;
        g_sampler = -1;
        g_tint = -1;
        return false;
    }
    g_program = program;
    return true;
}

void delete_textures() {
    for (TextureAsset& texture : g_textures) {
        if (texture.texture != 0 && glIsTexture(texture.texture) == GL_TRUE) {
            glDeleteTextures(1, &texture.texture);
        }
        texture = {};
    }
    g_texture_generation = 0;
}

bool upload_texture(const choose_hero_profile_assets::Asset& asset, TextureAsset* output) {
    if (output == nullptr || asset.width <= 0 || asset.height <= 0 ||
            asset.pixels.size() != static_cast<std::size_t>(asset.width) *
                static_cast<std::size_t>(asset.height)) {
        return false;
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
        return false;
    }

    output->texture = texture;
    output->asset = asset;
    return true;
}

bool ensure_textures() {
    const std::uint64_t generation = choose_hero_profile_assets::generation();
    if (g_texture_generation == generation &&
            g_textures[0].texture != 0 && glIsTexture(g_textures[0].texture) == GL_TRUE) {
        bool valid = true;
        for (std::uint32_t slot_id : {1u, 2u}) {
            if (!choose_hero_profile_assets::profile_ready(slot_id)) continue;
            for (int kind = 0; kind < choose_hero_profile_assets::kLabelCount; ++kind) {
                const TextureAsset& texture = g_textures[label_texture_index(slot_id, kind)];
                if (texture.texture == 0 || glIsTexture(texture.texture) != GL_TRUE) {
                    valid = false;
                    break;
                }
            }
            if (!valid) break;
        }
        if (valid) return true;
    }

    delete_textures();
    choose_hero_profile_assets::Asset background;
    if (!choose_hero_profile_assets::copy_background(&background) ||
            !upload_texture(background, &g_textures[0])) {
        delete_textures();
        return false;
    }

    for (std::uint32_t slot_id : {1u, 2u}) {
        if (!choose_hero_profile_assets::profile_ready(slot_id)) continue;
        for (int kind = 0; kind < choose_hero_profile_assets::kLabelCount; ++kind) {
            choose_hero_profile_assets::Asset label;
            if (!choose_hero_profile_assets::copy_label(slot_id, kind, &label) ||
                    !upload_texture(label, &g_textures[label_texture_index(slot_id, kind)])) {
                delete_textures();
                return false;
            }
        }
    }
    g_texture_generation = generation;
    return true;
}

void draw_texture(
        const TextureAsset& texture,
        float center_x,
        float center_y,
        float tint,
        const choose_hero_role_item_view::Mapping& mapping,
        int surface_width,
        int surface_height) {
    if (texture.texture == 0 || !mapping.valid || surface_width <= 0 || surface_height <= 0) return;

    const float half_width = static_cast<float>(texture.asset.width) * 0.5f;
    const float half_height = static_cast<float>(texture.asset.height) * 0.5f;
    const float left_px = mapping.offset_x + (center_x - half_width) * mapping.scale;
    const float right_px = mapping.offset_x + (center_x + half_width) * mapping.scale;
    const float bottom_px = mapping.offset_y + (center_y - half_height) * mapping.scale;
    const float top_px = mapping.offset_y + (center_y + half_height) * mapping.scale;

    const float width = static_cast<float>(surface_width);
    const float height = static_cast<float>(surface_height);
    const GLfloat x0 = 2.0f * left_px / width - 1.0f;
    const GLfloat x1 = 2.0f * right_px / width - 1.0f;
    const GLfloat y0 = 2.0f * bottom_px / height - 1.0f;
    const GLfloat y1 = 2.0f * top_px / height - 1.0f;
    const GLfloat vertices[] = {x0, y1, x0, y0, x1, y1, x1, y0};
    static constexpr GLfloat kTexCoords[] = {
        0.0f, 0.0f,
        0.0f, 1.0f,
        1.0f, 0.0f,
        1.0f, 1.0f,
    };

    glBindTexture(GL_TEXTURE_2D, texture.texture);
    glUniform1f(g_tint, tint);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, vertices);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 0, kTexCoords);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    g_draw_count.fetch_add(1, std::memory_order_relaxed);
}

}  // namespace

void draw() {
    if (!route_active() || !choose_hero_profile_assets::background_ready() ||
            !choose_hero_role_item_assets::ready() || !ensure_program() || !ensure_textures()) {
        return;
    }

    GLint viewport[4]{};
    glGetIntegerv(GL_VIEWPORT, viewport);
    const int surface_width = viewport[2];
    const int surface_height = viewport[3];
    if (surface_width <= 0 || surface_height <= 0) return;
    const auto mapping = choose_hero_role_item_view::mapping_for_surface(surface_width, surface_height);
    if (!mapping.valid) return;

    choose_hero_role_item_assets::Asset board;
    if (!choose_hero_role_item_assets::copy(choose_hero_role_item_assets::kBoardA, &board)) return;
    const auto state = choose_hero_role_selection_state::snapshot();

    glUseProgram(g_program);
    glActiveTexture(GL_TEXTURE0);
    glUniform1i(g_sampler, 0);
    glEnableVertexAttribArray(0);
    glEnableVertexAttribArray(1);

    for (std::size_t index = 0; index < state.items.size(); ++index) {
        const auto& item = state.items[index];
        if (item.kind != choose_hero_role_selection_state::ItemKind::kHero ||
                (item.tag != 1u && item.tag != 2u) ||
                !choose_hero_profile_assets::profile_ready(item.tag)) {
            continue;
        }

        choose_hero_role_selection_state::ItemPose pose;
        if (!choose_hero_role_selection_state::item_pose(
                index,
                choose_hero_role_item_view::kDesignHeight,
                static_cast<float>(board.height),
                &pose)) {
            continue;
        }

        const float left = pose.x - static_cast<float>(board.width) * 0.5f;
        const float bottom = pose.y - static_cast<float>(board.height) * 0.5f;
        const TextureAsset& background = g_textures[0];
        const float background_x = left + static_cast<float>(board.width) * 0.70f;
        const float background_y = bottom + static_cast<float>(board.height) * 0.67f;
        draw_texture(
            background,
            background_x,
            background_y,
            1.0f,
            mapping,
            surface_width,
            surface_height);

        const TextureAsset& name = g_textures[label_texture_index(
            item.tag, choose_hero_profile_assets::kName)];
        const TextureAsset& level = g_textures[label_texture_index(
            item.tag, choose_hero_profile_assets::kLevel)];
        const TextureAsset& play_time = g_textures[label_texture_index(
            item.tag, choose_hero_profile_assets::kPlayTime)];

        // Shipped name label anchor is (0, 0.5). Its X is measured from the
        // name-background left edge plus 20 design pixels.
        const float name_x = background_x - static_cast<float>(background.asset.width) * 0.5f + 20.0f;
        const float name_y = background_y;
        draw_texture(
            name,
            name_x + static_cast<float>(name.asset.width) * 0.5f,
            name_y,
            kTextTint,
            mapping,
            surface_width,
            surface_height);

        // The level label is a child of the name label at local
        // (nameLabel.contentWidth, 0), with anchor (0,0).
        draw_texture(
            level,
            name_x + static_cast<float>(name.asset.width) +
                static_cast<float>(level.asset.width) * 0.5f,
            name_y - static_cast<float>(name.asset.height) * 0.5f +
                static_cast<float>(level.asset.height) * 0.5f,
            kTextTint,
            mapping,
            surface_width,
            surface_height);

        // GameUSETime uses anchor (0,0.5) and the shipped X formula
        // 0.8*itemWidth - labelWidth/2, so its visible center is exactly 0.8W.
        draw_texture(
            play_time,
            left + static_cast<float>(board.width) * 0.80f,
            bottom + static_cast<float>(board.height) * 0.25f,
            kTextTint,
            mapping,
            surface_width,
            surface_height);
        g_profile_draw_count.fetch_add(1, std::memory_order_relaxed);
    }

    glDisableVertexAttribArray(1);
    glDisableVertexAttribArray(0);
    glBindTexture(GL_TEXTURE_2D, 0);
}

std::string status_report() {
    std::ostringstream out;
    out << "ChooseHero profile labels: " << (route_active() ? "active" : "inactive") << "\n";
    out << "ChooseHero profile background: "
        << (choose_hero_profile_assets::background_ready() ? "ready" : "not-ready")
        << " generation=" << choose_hero_profile_assets::generation() << "\n";
    out << "ChooseHero profiles ready: slot1="
        << (choose_hero_profile_assets::profile_ready(1u) ? "yes" : "no")
        << " slot2=" << (choose_hero_profile_assets::profile_ready(2u) ? "yes" : "no") << "\n";
    out << "ChooseHero profile draws: "
        << g_profile_draw_count.load(std::memory_order_relaxed)
        << " quads=" << g_draw_count.load(std::memory_order_relaxed) << "\n";
    return out.str();
}

}  // namespace nevergone::choose_hero_profile_compositor
