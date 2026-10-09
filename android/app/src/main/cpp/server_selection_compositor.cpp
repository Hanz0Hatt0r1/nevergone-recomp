#include "server_selection_compositor.h"

#include <GLES2/gl2.h>
#include <jni.h>

#include <array>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <sstream>
#include <vector>

#include "initial_ui_transition.h"
#include "server_selection_assets.h"
#include "server_selection_layout.h"
#include "server_selection_state.h"
#include "server_selection_view.h"

namespace nevergone::server_selection_compositor {
namespace {

struct TextureSlot {
    GLuint texture = 0;
    int width = 0;
    int height = 0;
};

std::atomic<int> g_surface_width{0};
std::atomic<int> g_surface_height{0};
std::atomic<int> g_selector_pointer{-1};
std::atomic<int> g_confirm_pointer{-1};
std::atomic<std::uint64_t> g_draw_count{0};
std::atomic<std::uint64_t> g_owned_touch_count{0};
GLuint g_color_program = 0;
GLint g_color_uniform = -1;
GLuint g_texture_program = 0;
GLint g_texture_sampler = -1;
std::array<TextureSlot, server_selection_assets::kAssetCount> g_asset_textures{};
std::uint64_t g_asset_texture_generation = 0;

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

GLuint build_color_program() {
    static constexpr const char* kVertex = R"GLSL(
attribute vec2 aPosition;
void main() {
    gl_Position = vec4(aPosition, 0.0, 1.0);
}
)GLSL";
    static constexpr const char* kFragment = R"GLSL(
precision mediump float;
uniform vec4 uColor;
void main() {
    gl_FragColor = uColor;
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

GLuint build_texture_program() {
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

bool route_active() {
    return nevergone::initial_ui_transition::snapshot().management_route ==
        nevergone::initial_ui_transition::ManagementRoute::kServerSelection;
}

bool finite_rect(const server_selection_layout::RowRect& rect) {
    return rect.index >= 0 &&
        std::isfinite(rect.left) && std::isfinite(rect.bottom) &&
        std::isfinite(rect.width) && std::isfinite(rect.height) &&
        rect.width > 0.0f && rect.height > 0.0f;
}

bool dimensions_or_fallback(
        int asset_index,
        float fallback_width,
        float fallback_height,
        float* width,
        float* height) {
    if (width == nullptr || height == nullptr) return false;
    int asset_width = 0;
    int asset_height = 0;
    if (nevergone::server_selection_assets::dimensions(
            asset_index, &asset_width, &asset_height)) {
        *width = static_cast<float>(asset_width);
        *height = static_cast<float>(asset_height);
        return true;
    }
    *width = fallback_width;
    *height = fallback_height;
    return false;
}

bool row_dimensions(float* width, float* height) {
    return dimensions_or_fallback(
        server_selection_assets::kBorder2,
        server_selection_view::kFallbackRowWidth,
        server_selection_view::kFallbackRowHeight,
        width,
        height);
}

bool selector_dimensions(float* width, float* height) {
    return dimensions_or_fallback(
        server_selection_assets::kBorder1,
        server_selection_view::kFallbackSelectorWidth,
        server_selection_view::kFallbackSelectorHeight,
        width,
        height);
}

bool confirm_dimensions(float* width, float* height) {
    return dimensions_or_fallback(
        server_selection_assets::kButtonNormal,
        server_selection_view::kFallbackConfirmWidth,
        server_selection_view::kFallbackConfirmHeight,
        width,
        height);
}

void rect_vertices(
        const server_selection_layout::RowRect& rect,
        GLfloat* vertices) {
    const int surface_width = g_surface_width.load(std::memory_order_relaxed);
    const int surface_height = g_surface_height.load(std::memory_order_relaxed);
    const auto mapping = nevergone::server_selection_view::mapping_for_surface(
        surface_width, surface_height);
    if (vertices == nullptr || !mapping.valid || !finite_rect(rect)) return;

    const float x0_px = mapping.offset_x + rect.left * mapping.scale;
    const float x1_px = mapping.offset_x + (rect.left + rect.width) * mapping.scale;
    const float y0_px = mapping.offset_y + rect.bottom * mapping.scale;
    const float y1_px = mapping.offset_y + (rect.bottom + rect.height) * mapping.scale;
    const float width = static_cast<float>(surface_width);
    const float height = static_cast<float>(surface_height);
    vertices[0] = 2.0f * x0_px / width - 1.0f;
    vertices[1] = 2.0f * y1_px / height - 1.0f;
    vertices[2] = vertices[0];
    vertices[3] = 2.0f * y0_px / height - 1.0f;
    vertices[4] = 2.0f * x1_px / width - 1.0f;
    vertices[5] = vertices[1];
    vertices[6] = vertices[4];
    vertices[7] = vertices[3];
}

void draw_rect(
        const server_selection_layout::RowRect& rect,
        float red,
        float green,
        float blue,
        float alpha) {
    const int surface_width = g_surface_width.load(std::memory_order_relaxed);
    const int surface_height = g_surface_height.load(std::memory_order_relaxed);
    const auto mapping = nevergone::server_selection_view::mapping_for_surface(
        surface_width, surface_height);
    if (g_color_program == 0 || g_color_uniform < 0 || !mapping.valid || !finite_rect(rect)) return;
    GLfloat vertices[8]{};
    rect_vertices(rect, vertices);
    glUseProgram(g_color_program);
    glUniform4f(g_color_uniform, red, green, blue, alpha);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, vertices);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    glDisableVertexAttribArray(0);
}

void clear_asset_textures() {
    for (TextureSlot& slot : g_asset_textures) {
        if (slot.texture != 0 && glIsTexture(slot.texture) == GL_TRUE) {
            glDeleteTextures(1, &slot.texture);
        }
        slot = {};
    }
    g_asset_texture_generation = 0;
}

bool upload_texture(
        const server_selection_assets::Asset& asset,
        TextureSlot* output) {
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
        GL_TEXTURE_2D, 0, GL_RGBA, asset.width, asset.height, 0,
        GL_RGBA, GL_UNSIGNED_BYTE, rgba.data());
    const GLenum error = glGetError();
    glBindTexture(GL_TEXTURE_2D, 0);
    if (error != GL_NO_ERROR) {
        glDeleteTextures(1, &texture);
        return false;
    }
    output->texture = texture;
    output->width = asset.width;
    output->height = asset.height;
    return true;
}

bool ensure_asset_textures() {
    if (g_texture_program == 0 || g_texture_sampler < 0) return false;
    const std::uint64_t generation = nevergone::server_selection_assets::generation();
    if (g_asset_texture_generation == generation) {
        bool valid = true;
        for (int index = 0; index < server_selection_assets::kAssetCount; ++index) {
            if (!server_selection_assets::ready(index)) continue;
            const TextureSlot& slot = g_asset_textures[static_cast<std::size_t>(index)];
            if (slot.texture == 0 || glIsTexture(slot.texture) != GL_TRUE) {
                valid = false;
                break;
            }
        }
        if (valid) return true;
    }

    clear_asset_textures();
    for (int index = 0; index < server_selection_assets::kAssetCount; ++index) {
        if (!server_selection_assets::ready(index)) continue;
        server_selection_assets::Asset asset;
        if (!server_selection_assets::copy(index, &asset) ||
                !upload_texture(asset, &g_asset_textures[static_cast<std::size_t>(index)])) {
            clear_asset_textures();
            return false;
        }
    }
    g_asset_texture_generation = generation;
    return true;
}

void draw_texture(
        const TextureSlot& slot,
        const server_selection_layout::RowRect& rect) {
    if (g_texture_program == 0 || g_texture_sampler < 0 || slot.texture == 0 ||
            !finite_rect(rect)) {
        return;
    }
    GLfloat vertices[8]{};
    rect_vertices(rect, vertices);
    static constexpr GLfloat kTexCoords[] = {
        0.0f, 0.0f,
        0.0f, 1.0f,
        1.0f, 0.0f,
        1.0f, 1.0f,
    };
    const GLboolean blend_was_enabled = glIsEnabled(GL_BLEND);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glUseProgram(g_texture_program);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, slot.texture);
    glUniform1i(g_texture_sampler, 0);
    glEnableVertexAttribArray(0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, vertices);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 0, kTexCoords);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    glDisableVertexAttribArray(1);
    glDisableVertexAttribArray(0);
    glBindTexture(GL_TEXTURE_2D, 0);
    if (blend_was_enabled == GL_FALSE) glDisable(GL_BLEND);
}

server_selection_layout::RowRect panel_rect() {
    server_selection_layout::RowRect rect;
    rect.index = 0;
    rect.left = 24.0f;
    rect.bottom = 24.0f;
    rect.width = server_selection_layout::kDesignWidth - 48.0f;
    rect.height = server_selection_layout::kDesignHeight - 48.0f;
    return rect;
}

server_selection_layout::RowRect label_rect(const TextureSlot& label) {
    server_selection_layout::RowRect rect;
    if (label.width <= 0 || label.height <= 0) return rect;
    constexpr float kType1LabelMaxWidth = 115.0f;
    const float width = static_cast<float>(label.width);
    const float scale = width > kType1LabelMaxWidth ? kType1LabelMaxWidth / width : 1.0f;
    rect.index = 0;
    rect.left = server_selection_view::kConfirmCenterX - width * scale * 0.5f;
    rect.bottom = server_selection_view::kConfirmCenterY -
        static_cast<float>(label.height) * scale * 0.5f;
    rect.width = width * scale;
    rect.height = static_cast<float>(label.height) * scale;
    return rect;
}

void clear_pointer(std::atomic<int>* pointer, int pointer_id) {
    if (pointer == nullptr) return;
    int expected = pointer_id;
    pointer->compare_exchange_strong(expected, -1, std::memory_order_relaxed);
}

void clear_touch(int pointer_id) {
    nevergone::server_selection_state::touch_cancelled(pointer_id);
    clear_pointer(&g_selector_pointer, pointer_id);
    clear_pointer(&g_confirm_pointer, pointer_id);
}

}  // namespace

void on_surface_created() {
    g_color_program = 0;
    g_color_uniform = -1;
    g_texture_program = 0;
    g_texture_sampler = -1;
    g_asset_textures = {};
    g_asset_texture_generation = 0;
    g_selector_pointer.store(-1, std::memory_order_relaxed);
    g_confirm_pointer.store(-1, std::memory_order_relaxed);

    const GLuint color_program = build_color_program();
    if (color_program != 0) {
        const GLint color = glGetUniformLocation(color_program, "uColor");
        if (color >= 0) {
            g_color_program = color_program;
            g_color_uniform = color;
        } else {
            glDeleteProgram(color_program);
        }
    }

    const GLuint texture_program = build_texture_program();
    if (texture_program != 0) {
        const GLint sampler = glGetUniformLocation(texture_program, "uTexture");
        if (sampler >= 0) {
            g_texture_program = texture_program;
            g_texture_sampler = sampler;
        } else {
            glDeleteProgram(texture_program);
        }
    }
}

void on_surface_changed(int width, int height) {
    g_surface_width.store(width, std::memory_order_relaxed);
    g_surface_height.store(height, std::memory_order_relaxed);
}

bool active() {
    const auto state = nevergone::server_selection_state::snapshot();
    return route_active() && state.payload_valid && state.server_count > 0;
}

void draw() {
    if (!active() || g_color_program == 0) return;

    const auto state = nevergone::server_selection_state::snapshot();
    const bool textures_ready = ensure_asset_textures();

    if (state.chooser_open) {
        // The original hidden CCLayerColor becomes visible at tag 10001 and is
        // raised to z-order 5. The surrounding panel remains project-owned,
        // while row placement/artwork are recovered from NewServerList.
        draw_rect(panel_rect(), 0.035f, 0.055f, 0.085f, 0.82f);
        float row_width = server_selection_view::kFallbackRowWidth;
        float row_height = server_selection_view::kFallbackRowHeight;
        const bool original_rows = row_dimensions(&row_width, &row_height) &&
            textures_ready &&
            g_asset_textures[server_selection_assets::kBorder2].texture != 0;
        const auto rows = server_selection_layout::build_rows(
            state.server_count, row_width, row_height);
        for (const auto& row : rows) {
            if (row.bottom + row.height < 0.0f ||
                    row.bottom > server_selection_layout::kDesignHeight) {
                continue;
            }
            const bool selected = row.index == state.selected_index;
            if (original_rows) {
                draw_texture(g_asset_textures[server_selection_assets::kBorder2], row);
                if (selected) draw_rect(row, 0.20f, 0.58f, 0.90f, 0.24f);
            } else if (selected) {
                draw_rect(row, 0.20f, 0.58f, 0.90f, 0.92f);
            } else {
                draw_rect(row, 0.12f, 0.18f, 0.28f, 0.88f);
            }
        }
        g_draw_count.fetch_add(1, std::memory_order_relaxed);
        return;
    }

    // Main NewServerList view: border1 tag 10001 is the selector at y=200.
    float selector_width = server_selection_view::kFallbackSelectorWidth;
    float selector_height = server_selection_view::kFallbackSelectorHeight;
    const bool original_selector = selector_dimensions(&selector_width, &selector_height) &&
        textures_ready && g_asset_textures[server_selection_assets::kBorder1].texture != 0;
    const auto selector = server_selection_view::selector_rect(selector_width, selector_height);
    if (original_selector) {
        // The original uses border1 for both normal and selected images.
        draw_texture(g_asset_textures[server_selection_assets::kBorder1], selector);
    } else {
        draw_rect(selector, 0.12f, 0.18f, 0.28f, 0.92f);
    }

    if (state.selected_index >= 0) {
        float confirm_width = server_selection_view::kFallbackConfirmWidth;
        float confirm_height = server_selection_view::kFallbackConfirmHeight;
        const bool original_confirm = confirm_dimensions(&confirm_width, &confirm_height) &&
            textures_ready && server_selection_assets::confirm_ready();
        const auto confirm = server_selection_view::confirm_rect(confirm_width, confirm_height);
        if (original_confirm) {
            const int button_asset = g_confirm_pointer.load(std::memory_order_relaxed) >= 0
                ? server_selection_assets::kButtonPressed
                : server_selection_assets::kButtonNormal;
            draw_texture(g_asset_textures[static_cast<std::size_t>(button_asset)], confirm);
            draw_texture(
                g_asset_textures[server_selection_assets::kStartLabel],
                label_rect(g_asset_textures[server_selection_assets::kStartLabel]));
        } else if (state.enter_request_pending) {
            draw_rect(confirm, 0.20f, 0.72f, 0.38f, 0.95f);
        } else {
            draw_rect(confirm, 0.82f, 0.48f, 0.16f, 0.95f);
        }
    }

    g_draw_count.fetch_add(1, std::memory_order_relaxed);
}

bool on_touch(int action, int pointer_id, float surface_x, float surface_y) {
    if (!active()) {
        clear_touch(pointer_id);
        return false;
    }

    const int width = g_surface_width.load(std::memory_order_relaxed);
    const int height = g_surface_height.load(std::memory_order_relaxed);
    const auto point = server_selection_view::surface_to_design(
        width, height, surface_x, surface_y);
    if (!point.inside_design) {
        if (action == 1 || action == 3 || action == 6) clear_touch(pointer_id);
        return true;
    }

    g_owned_touch_count.fetch_add(1, std::memory_order_relaxed);
    const auto state = server_selection_state::snapshot();

    // Android MotionEvent constants: DOWN=0, UP=1, MOVE=2, CANCEL=3,
    // POINTER_DOWN=5, POINTER_UP=6.
    if (state.chooser_open) {
        if (action == 0 || action == 5) {
            server_selection_state::touch_began(pointer_id, point.y);
            return true;
        }
        if (action == 3) {
            clear_touch(pointer_id);
            return true;
        }
        if (action == 1 || action == 6) {
            float row_width = server_selection_view::kFallbackRowWidth;
            float row_height = server_selection_view::kFallbackRowHeight;
            (void)row_dimensions(&row_width, &row_height);
            const int hit_index = server_selection_layout::hit_test(
                state.server_count, row_width, row_height, point.x, point.y);
            (void)server_selection_state::touch_ended(pointer_id, point.y, hit_index);
            clear_pointer(&g_selector_pointer, pointer_id);
            clear_pointer(&g_confirm_pointer, pointer_id);
            return true;
        }
        return true;
    }

    float selector_width = server_selection_view::kFallbackSelectorWidth;
    float selector_height = server_selection_view::kFallbackSelectorHeight;
    (void)selector_dimensions(&selector_width, &selector_height);
    float confirm_width = server_selection_view::kFallbackConfirmWidth;
    float confirm_height = server_selection_view::kFallbackConfirmHeight;
    (void)confirm_dimensions(&confirm_width, &confirm_height);

    if (action == 0 || action == 5) {
        if (server_selection_view::selector_contains(
                point.x, point.y, selector_width, selector_height)) {
            g_selector_pointer.store(pointer_id, std::memory_order_relaxed);
            g_confirm_pointer.store(-1, std::memory_order_relaxed);
            return true;
        }
        if (state.selected_index >= 0 &&
                server_selection_view::confirm_contains(
                    point.x, point.y, confirm_width, confirm_height)) {
            g_confirm_pointer.store(pointer_id, std::memory_order_relaxed);
            g_selector_pointer.store(-1, std::memory_order_relaxed);
            return true;
        }
        return true;
    }

    if (action == 3) {
        clear_touch(pointer_id);
        return true;
    }

    if (action == 1 || action == 6) {
        const int selector_pointer = g_selector_pointer.load(std::memory_order_relaxed);
        if (selector_pointer == pointer_id &&
                server_selection_view::selector_contains(
                    point.x, point.y, selector_width, selector_height)) {
            clear_pointer(&g_selector_pointer, pointer_id);
            (void)server_selection_state::open_chooser();
            return true;
        }

        const int confirm_pointer = g_confirm_pointer.load(std::memory_order_relaxed);
        if (confirm_pointer == pointer_id &&
                server_selection_view::confirm_contains(
                    point.x, point.y, confirm_width, confirm_height)) {
            clear_pointer(&g_confirm_pointer, pointer_id);
            (void)server_selection_state::confirm_selection();
            return true;
        }

        clear_touch(pointer_id);
        return true;
    }

    return true;
}

std::string status_report() {
    std::ostringstream out;
    const auto state = server_selection_state::snapshot();
    out << "server selection compositor: " << (active() ? "active" : "inactive") << "\n";
    out << "server selection surface: "
        << g_surface_width.load(std::memory_order_relaxed) << "x"
        << g_surface_height.load(std::memory_order_relaxed) << "\n";
    out << "server selection visible view: "
        << (state.chooser_open ? "chooser-overlay" : "main-selector") << "\n";

    int selector_width = 0;
    int selector_height = 0;
    if (server_selection_assets::dimensions(
            server_selection_assets::kBorder1, &selector_width, &selector_height)) {
        out << "server selection selector visual: original OBB border1 "
            << selector_width << "x" << selector_height << " center="
            << server_selection_view::kSelectorCenterX << ","
            << server_selection_view::kSelectorCenterY << "\n";
    } else {
        out << "server selection selector visual: project-owned fallback center="
            << server_selection_view::kSelectorCenterX << ","
            << server_selection_view::kSelectorCenterY << "\n";
    }

    int row_width = 0;
    int row_height = 0;
    if (server_selection_assets::dimensions(
            server_selection_assets::kBorder2, &row_width, &row_height)) {
        out << "server selection row visual: original OBB border2 "
            << row_width << "x" << row_height << "\n";
    } else {
        out << "server selection row visual: project-owned fallback row="
            << server_selection_view::kFallbackRowWidth
            << "x" << server_selection_view::kFallbackRowHeight << "\n";
    }

    int confirm_width = 0;
    int confirm_height = 0;
    if (server_selection_assets::dimensions(
            server_selection_assets::kButtonNormal,
            &confirm_width,
            &confirm_height) && server_selection_assets::confirm_ready()) {
        out << "server selection confirm visual: original standard button "
            << confirm_width << "x" << confirm_height << " center="
            << server_selection_view::kConfirmCenterX << ","
            << server_selection_view::kConfirmCenterY << "\n";
    } else {
        out << "server selection confirm visual: project-owned fallback center="
            << server_selection_view::kConfirmCenterX << ","
            << server_selection_view::kConfirmCenterY << "\n";
    }
    out << "server selection draws: " << g_draw_count.load(std::memory_order_relaxed)
        << " owned-touches=" << g_owned_touch_count.load(std::memory_order_relaxed) << "\n";
    return out.str();
}

}  // namespace nevergone::server_selection_compositor

extern "C" JNIEXPORT void JNICALL
Java_org_nevergone_recomp_GameSurfaceView_nativeOnServerSelectionSurfaceCreated(JNIEnv*, jclass) {
    nevergone::server_selection_compositor::on_surface_created();
}

extern "C" JNIEXPORT void JNICALL
Java_org_nevergone_recomp_GameSurfaceView_nativeOnServerSelectionSurfaceChanged(
        JNIEnv*, jclass, jint width, jint height) {
    nevergone::server_selection_compositor::on_surface_changed(
        static_cast<int>(width), static_cast<int>(height));
}

extern "C" JNIEXPORT void JNICALL
Java_org_nevergone_recomp_GameSurfaceView_nativeDrawServerSelectionLayer(JNIEnv*, jclass) {
    nevergone::server_selection_compositor::draw();
}
