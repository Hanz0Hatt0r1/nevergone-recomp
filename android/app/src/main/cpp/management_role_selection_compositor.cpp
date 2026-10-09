#include "management_role_selection_compositor.h"

#include <GLES2/gl2.h>
#include <jni.h>

#include <array>
#include <atomic>
#include <cstdint>
#include <sstream>
#include <vector>

#include "choose_hero_role_item_assets.h"
#include "choose_hero_role_item_view.h"
#include "client_callback_bridge.h"
#include "initial_ui_transition.h"
#include "management_role_selection_view.h"
#include "role_selection_state.h"

namespace nevergone::management_role_selection_compositor {
namespace {

struct TextureAsset {
    GLuint texture = 0;
    choose_hero_role_item_assets::Asset asset;
};

GLuint g_program = 0;
GLint g_sampler = -1;
std::array<TextureAsset, choose_hero_role_item_assets::kAssetCount> g_textures{};
std::uint64_t g_texture_generation = 0;
std::atomic<int> g_surface_width{0};
std::atomic<int> g_surface_height{0};
std::atomic<int> g_pointer{-1};
std::atomic<int> g_down_index{-1};
std::atomic<std::uint64_t> g_draw_count{0};
std::atomic<std::uint64_t> g_touch_count{0};
std::atomic<std::uint64_t> g_selection_dispatch_count{0};

bool route_active() {
    return initial_ui_transition::snapshot().management_route ==
        initial_ui_transition::ManagementRoute::kRoleSelection;
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
    if (g_sampler < 0) {
        glDeleteProgram(program);
        g_program = 0;
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

bool upload_texture(TextureAsset* output, int index) {
    if (output == nullptr) return false;
    choose_hero_role_item_assets::Asset asset;
    if (!choose_hero_role_item_assets::copy(index, &asset) ||
            asset.width <= 0 || asset.height <= 0 ||
            asset.pixels.size() != static_cast<std::size_t>(asset.width) *
                static_cast<std::size_t>(asset.height)) {
        return false;
    }

    std::vector<std::uint8_t> rgba(asset.pixels.size() * 4u);
    for (std::size_t index_pixel = 0; index_pixel < asset.pixels.size(); ++index_pixel) {
        const std::uint32_t pixel = asset.pixels[index_pixel];
        rgba[index_pixel * 4u] = static_cast<std::uint8_t>((pixel >> 16u) & 0xffu);
        rgba[index_pixel * 4u + 1u] = static_cast<std::uint8_t>((pixel >> 8u) & 0xffu);
        rgba[index_pixel * 4u + 2u] = static_cast<std::uint8_t>(pixel & 0xffu);
        rgba[index_pixel * 4u + 3u] = static_cast<std::uint8_t>((pixel >> 24u) & 0xffu);
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
    output->asset = std::move(asset);
    return true;
}

bool ensure_textures() {
    const std::uint64_t generation = choose_hero_role_item_assets::generation();
    if (g_texture_generation == generation) {
        bool valid = true;
        for (const TextureAsset& texture : g_textures) {
            if (texture.texture == 0 || glIsTexture(texture.texture) != GL_TRUE) {
                valid = false;
                break;
            }
        }
        if (valid) return true;
    }

    delete_textures();
    if (!choose_hero_role_item_assets::ready()) return false;
    for (int index = 0; index < choose_hero_role_item_assets::kAssetCount; ++index) {
        if (!upload_texture(&g_textures[static_cast<std::size_t>(index)], index)) {
            delete_textures();
            return false;
        }
    }
    g_texture_generation = generation;
    return true;
}

int board_index(bool selected) {
    return selected
        ? choose_hero_role_item_assets::kBoardB
        : choose_hero_role_item_assets::kBoardA;
}

int hero_icon_index(std::int64_t career, bool selected) {
    if (career == 1) {
        return selected
            ? choose_hero_role_item_assets::kHero01B
            : choose_hero_role_item_assets::kHero01A;
    }
    if (career == 2) {
        return selected
            ? choose_hero_role_item_assets::kHero02B
            : choose_hero_role_item_assets::kHero02A;
    }
    return -1;
}

void draw_texture(
        const TextureAsset& texture,
        float center_x,
        float center_y,
        const choose_hero_role_item_view::Mapping& mapping,
        int surface_width,
        int surface_height) {
    if (texture.texture == 0 || !mapping.valid || surface_width <= 0 || surface_height <= 0) {
        return;
    }

    const float half_width = static_cast<float>(texture.asset.width) * 0.5f;
    const float half_height = static_cast<float>(texture.asset.height) * 0.5f;
    const float left_px = mapping.offset_x + (center_x - half_width) * mapping.scale;
    const float right_px = mapping.offset_x + (center_x + half_width) * mapping.scale;
    const float bottom_px = mapping.offset_y + (center_y - half_height) * mapping.scale;
    const float top_px = mapping.offset_y + (center_y + half_height) * mapping.scale;

    const float width = static_cast<float>(surface_width);
    const float height = static_cast<float>(surface_height);
    const GLfloat x0 = (2.0f * left_px / width) - 1.0f;
    const GLfloat x1 = (2.0f * right_px / width) - 1.0f;
    const GLfloat y0 = (2.0f * bottom_px / height) - 1.0f;
    const GLfloat y1 = (2.0f * top_px / height) - 1.0f;
    const GLfloat vertices[] = {x0, y1, x0, y0, x1, y1, x1, y0};
    static constexpr GLfloat kTexCoords[] = {
        0.0f, 0.0f,
        0.0f, 1.0f,
        1.0f, 0.0f,
        1.0f, 1.0f,
    };

    glBindTexture(GL_TEXTURE_2D, texture.texture);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, vertices);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 0, kTexCoords);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    g_draw_count.fetch_add(1, std::memory_order_relaxed);
}

int hit_test(float surface_x, float surface_y) {
    if (!route_active() || !choose_hero_role_item_assets::ready()) return -1;
    const int surface_width = g_surface_width.load(std::memory_order_relaxed);
    const int surface_height = g_surface_height.load(std::memory_order_relaxed);
    const auto point = choose_hero_role_item_view::surface_to_design(
        surface_width, surface_height, surface_x, surface_y);
    if (!point.inside) return -1;

    choose_hero_role_item_assets::Asset board;
    if (!choose_hero_role_item_assets::copy(choose_hero_role_item_assets::kBoardA, &board)) {
        return -1;
    }
    const auto ui = lua_runtime::snapshot_client_ui_state();
    if (!ui.role_list_model.valid) return -1;
    return management_role_selection_view::hit_test(
        ui.role_list_model.roles.size(),
        choose_hero_role_item_view::kDesignHeight,
        static_cast<float>(board.width),
        static_cast<float>(board.height),
        point.x,
        point.y);
}

void clear_pointer(int pointer_id) {
    int expected = pointer_id;
    if (g_pointer.compare_exchange_strong(expected, -1, std::memory_order_relaxed)) {
        g_down_index.store(-1, std::memory_order_relaxed);
    }
}

}  // namespace

void draw() {
    if (!route_active() || !choose_hero_role_item_assets::ready() ||
            !ensure_program() || !ensure_textures()) {
        return;
    }

    const auto ui = lua_runtime::snapshot_client_ui_state();
    if (!ui.role_list_model.valid || ui.role_list_model.roles.empty()) return;

    GLint viewport[4]{};
    glGetIntegerv(GL_VIEWPORT, viewport);
    const int surface_width = viewport[2];
    const int surface_height = viewport[3];
    if (surface_width <= 0 || surface_height <= 0) return;
    g_surface_width.store(surface_width, std::memory_order_relaxed);
    g_surface_height.store(surface_height, std::memory_order_relaxed);

    const auto mapping = choose_hero_role_item_view::mapping_for_surface(
        surface_width, surface_height);
    if (!mapping.valid) return;

    const auto selection = role_selection_state::snapshot();
    const auto& board = g_textures[choose_hero_role_item_assets::kBoardA].asset;
    if (board.height <= 0) return;

    glUseProgram(g_program);
    glActiveTexture(GL_TEXTURE0);
    glUniform1i(g_sampler, 0);
    glEnableVertexAttribArray(0);
    glEnableVertexAttribArray(1);

    for (std::size_t index = 0; index < ui.role_list_model.roles.size(); ++index) {
        const auto pose = management_role_selection_view::item_pose(
            index,
            choose_hero_role_item_view::kDesignHeight,
            static_cast<float>(board.height));
        if (!pose.valid) continue;

        const bool selected = selection.selected_index == static_cast<int>(index);
        draw_texture(
            g_textures[static_cast<std::size_t>(board_index(selected))],
            pose.x,
            pose.y,
            mapping,
            surface_width,
            surface_height);

        const auto& role = ui.role_list_model.roles[index];
        const int icon = hero_icon_index(role.career, selected);
        if (icon >= 0) {
            draw_texture(
                g_textures[static_cast<std::size_t>(icon)],
                pose.x - 30.0f,
                pose.y,
                mapping,
                surface_width,
                surface_height);
        }
    }

    glDisableVertexAttribArray(1);
    glDisableVertexAttribArray(0);
    glBindTexture(GL_TEXTURE_2D, 0);
}

bool on_touch(int action, int pointer_id, float surface_x, float surface_y) {
    if (!route_active() || !choose_hero_role_item_assets::ready()) return false;

    // Android MotionEvent actions: DOWN=0, UP=1, MOVE=2, CANCEL=3,
    // POINTER_DOWN=5, POINTER_UP=6.
    if (action == 0 || action == 5) {
        const int index = hit_test(surface_x, surface_y);
        if (index < 0) return false;
        int expected = -1;
        if (!g_pointer.compare_exchange_strong(
                expected, pointer_id, std::memory_order_relaxed)) {
            return false;
        }
        g_down_index.store(index, std::memory_order_relaxed);
        g_touch_count.fetch_add(1, std::memory_order_relaxed);
        return true;
    }

    if (g_pointer.load(std::memory_order_relaxed) != pointer_id) return false;
    g_touch_count.fetch_add(1, std::memory_order_relaxed);

    if (action == 2) return true;
    if (action == 3) {
        clear_pointer(pointer_id);
        return true;
    }
    if (action == 1 || action == 6) {
        const int down_index = g_down_index.load(std::memory_order_relaxed);
        const int up_index = hit_test(surface_x, surface_y);
        if (down_index >= 0 && up_index == down_index &&
                role_selection_state::select_index(up_index)) {
            g_selection_dispatch_count.fetch_add(1, std::memory_order_relaxed);
        }
        clear_pointer(pointer_id);
        return true;
    }
    return true;
}

std::string status_report() {
    std::ostringstream out;
    const auto ui = lua_runtime::snapshot_client_ui_state();
    const auto selection = role_selection_state::snapshot();
    out << "management role compositor: " << (route_active() ? "active" : "inactive")
        << " roles=" << (ui.role_list_model.valid ? ui.role_list_model.roles.size() : 0)
        << " selected=" << selection.selected_index << "\n";
    out << "management role draws: " << g_draw_count.load(std::memory_order_relaxed)
        << " touches=" << g_touch_count.load(std::memory_order_relaxed)
        << " selections=" << g_selection_dispatch_count.load(std::memory_order_relaxed)
        << "\n";
    return out.str();
}

}  // namespace nevergone::management_role_selection_compositor

extern "C" JNIEXPORT void JNICALL
Java_org_nevergone_recomp_GameSurfaceView_nativeDrawManagementRoleSelectionLayer(
        JNIEnv*, jclass) {
    nevergone::management_role_selection_compositor::draw();
}

extern "C" JNIEXPORT jboolean JNICALL
Java_org_nevergone_recomp_GameSurfaceView_nativeOnManagementRoleSelectionTouch(
        JNIEnv*, jclass, jint action, jint pointer_id, jfloat x, jfloat y) {
    return nevergone::management_role_selection_compositor::on_touch(
        static_cast<int>(action),
        static_cast<int>(pointer_id),
        static_cast<float>(x),
        static_cast<float>(y))
        ? JNI_TRUE
        : JNI_FALSE;
}
