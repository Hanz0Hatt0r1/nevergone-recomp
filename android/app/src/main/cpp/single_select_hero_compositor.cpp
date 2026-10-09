#include <GLES2/gl2.h>
#include <jni.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

#include "offline_startup_flow.h"
#include "single_select_hero_state.h"

namespace nevergone::single_select_hero {
namespace {

constexpr int kHeroTableCount = 4;
constexpr float kDesignWidth = 1136.0f;
constexpr float kDesignHeight = 640.0f;
constexpr float kHeroTableCenterX = 830.0f;
constexpr float kHeroTableCenterY = 320.0f;

struct PositionedTexture {
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
PositionedTexture g_background{};
std::vector<std::uint32_t> g_background_pixels;
PositionedTexture g_hero_tables[kHeroTableCount]{};
std::vector<std::uint32_t> g_hero_table_pixels[kHeroTableCount];
int g_surface_width = 0;
int g_surface_height = 0;

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

void delete_texture(PositionedTexture* asset) {
    if (asset != nullptr && asset->texture != 0) {
        glDeleteTextures(1, &asset->texture);
        asset->texture = 0;
    }
}

void clear_background_asset() {
    delete_texture(&g_background);
    g_background = {};
    g_background_pixels.clear();
    g_background_pixels.shrink_to_fit();
}

void clear_hero_tables() {
    for (int index = 0; index < kHeroTableCount; ++index) {
        delete_texture(&g_hero_tables[index]);
        g_hero_tables[index] = {};
        g_hero_table_pixels[index].clear();
        g_hero_table_pixels[index].shrink_to_fit();
    }
}

GLuint create_texture(const std::uint32_t* argb, size_t count, int width, int height) {
    if (argb == nullptr || width <= 0 || height <= 0 ||
            count != static_cast<size_t>(width) * static_cast<size_t>(height) ||
            count > 16777216u) {
        return 0;
    }

    std::vector<std::uint8_t> rgba(count * 4u);
    for (size_t index = 0; index < count; ++index) {
        const std::uint32_t pixel = argb[index];
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
            GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0,
            GL_RGBA, GL_UNSIGNED_BYTE, rgba.data());
    const GLenum error = glGetError();
    glBindTexture(GL_TEXTURE_2D, 0);
    if (error != GL_NO_ERROR) {
        glDeleteTextures(1, &texture);
        return 0;
    }
    return texture;
}

bool ensure_texture(PositionedTexture* asset, const std::vector<std::uint32_t>& pixels) {
    if (asset == nullptr) return false;
    if (asset->texture != 0) return true;
    if (asset->width <= 0 || asset->height <= 0 ||
            pixels.size() != static_cast<size_t>(asset->width) * static_cast<size_t>(asset->height)) {
        return false;
    }
    asset->texture = create_texture(pixels.data(), pixels.size(), asset->width, asset->height);
    return asset->texture != 0;
}

bool ensure_background_texture() {
    return ensure_texture(&g_background, g_background_pixels);
}

void on_surface_created() {
    // A recreated GL context invalidates texture names, while CPU backing stays
    // available until Java refreshes the imported atlas frames.
    g_background.texture = 0;
    for (auto& table : g_hero_tables) table.texture = 0;
    if (g_program != 0) glDeleteProgram(g_program);
    g_program = build_program();
    g_sampler = g_program != 0 ? glGetUniformLocation(g_program, "uTexture") : -1;
}

void on_surface_changed(int width, int height) {
    g_surface_width = width;
    g_surface_height = height;
}

bool validate_asset_geometry(
        int width,
        int height,
        int left,
        int top,
        int source_width,
        int source_height,
        const std::uint32_t* argb,
        size_t count) {
    return g_program != 0 && width > 0 && height > 0 &&
        source_width > 0 && source_height > 0 && left >= 0 && top >= 0 &&
        left + width <= source_width && top + height <= source_height && argb != nullptr &&
        count == static_cast<size_t>(width) * static_cast<size_t>(height) &&
        count <= 16777216u;
}

bool upload_background(
        int width,
        int height,
        int left,
        int top,
        int source_width,
        int source_height,
        const std::uint32_t* argb,
        size_t count) {
    if (!validate_asset_geometry(
            width, height, left, top, source_width, source_height, argb, count)) {
        return false;
    }

    clear_background_asset();
    g_background.width = width;
    g_background.height = height;
    g_background.left = left;
    g_background.top = top;
    g_background.source_width = source_width;
    g_background.source_height = source_height;
    g_background_pixels.assign(argb, argb + count);

    if (offline_startup_flow::snapshot().route != offline_startup_flow::Route::kOpeningDialogue) {
        return true;
    }
    return ensure_background_texture();
}

bool upload_hero_table(
        int table_index,
        int width,
        int height,
        int left,
        int top,
        int source_width,
        int source_height,
        const std::uint32_t* argb,
        size_t count) {
    if (table_index < 0 || table_index >= kHeroTableCount ||
            !validate_asset_geometry(
                    width, height, left, top, source_width, source_height, argb, count)) {
        return false;
    }

    PositionedTexture& asset = g_hero_tables[table_index];
    delete_texture(&asset);
    asset = {};
    asset.width = width;
    asset.height = height;
    asset.left = left;
    asset.top = top;
    asset.source_width = source_width;
    asset.source_height = source_height;
    g_hero_table_pixels[table_index].assign(argb, argb + count);

    if (offline_startup_flow::snapshot().route != offline_startup_flow::Route::kOpeningDialogue) {
        return true;
    }
    return ensure_texture(&asset, g_hero_table_pixels[table_index]);
}

void bind_and_draw(GLuint texture, const GLfloat* vertices, bool blend) {
    static constexpr GLfloat kTexCoords[] = {
            0.0f, 0.0f,
            0.0f, 1.0f,
            1.0f, 0.0f,
            1.0f, 1.0f,
    };

    if (blend) {
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    }
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
    if (blend) glDisable(GL_BLEND);
}

void draw_background() {
    if (!ensure_background_texture()) return;

    const float image_aspect = static_cast<float>(g_background.source_width) /
            static_cast<float>(g_background.source_height);
    const float surface_aspect = static_cast<float>(g_surface_width) /
            static_cast<float>(g_surface_height);
    float half_width = 1.0f;
    float half_height = 1.0f;
    if (image_aspect > surface_aspect) {
        half_height = surface_aspect / image_aspect;
    } else {
        half_width = image_aspect / surface_aspect;
    }

    const float x0 = -half_width + 2.0f * half_width *
            static_cast<float>(g_background.left) / static_cast<float>(g_background.source_width);
    const float x1 = -half_width + 2.0f * half_width *
            static_cast<float>(g_background.left + g_background.width) /
            static_cast<float>(g_background.source_width);
    const float y0 = half_height - 2.0f * half_height *
            static_cast<float>(g_background.top) / static_cast<float>(g_background.source_height);
    const float y1 = half_height - 2.0f * half_height *
            static_cast<float>(g_background.top + g_background.height) /
            static_cast<float>(g_background.source_height);
    const GLfloat vertices[] = {x0, y0, x0, y1, x1, y0, x1, y1};
    bind_and_draw(g_background.texture, vertices, false);
}

void draw_hero_table() {
    const auto state = single_select_hero_state::snapshot();
    if (!state.active || !single_select_hero_state::is_valid_career(state.selected_career)) return;

    const bool blocked = state.existing_career != 0 &&
        state.existing_career == state.selected_career;
    const int table_index = static_cast<int>((state.selected_career - 1) * 2 + (blocked ? 1 : 0));
    PositionedTexture& asset = g_hero_tables[table_index];
    if (!ensure_texture(&asset, g_hero_table_pixels[table_index])) return;

    const float scale = std::min(
        static_cast<float>(g_surface_width) / kDesignWidth,
        static_cast<float>(g_surface_height) / kDesignHeight);
    if (!std::isfinite(scale) || scale <= 0.0f) return;
    const float viewport_width = kDesignWidth * scale;
    const float viewport_height = kDesignHeight * scale;
    const float offset_x = (static_cast<float>(g_surface_width) - viewport_width) * 0.5f;
    const float offset_y = (static_cast<float>(g_surface_height) - viewport_height) * 0.5f;

    // TexturePacker placement.left/top are the trimmed-image offsets inside the
    // untrimmed source rectangle. The Cocos sprite itself is centered at the
    // recovered design coordinate (830, 320), with Y increasing upward.
    const float design_left = kHeroTableCenterX -
        static_cast<float>(asset.source_width) * 0.5f + static_cast<float>(asset.left);
    const float design_top = kDesignHeight -
        (kHeroTableCenterY + static_cast<float>(asset.source_height) * 0.5f) +
        static_cast<float>(asset.top);
    const float design_right = design_left + static_cast<float>(asset.width);
    const float design_bottom = design_top + static_cast<float>(asset.height);

    const float surface_left = offset_x + design_left * scale;
    const float surface_right = offset_x + design_right * scale;
    const float surface_top = offset_y + design_top * scale;
    const float surface_bottom = offset_y + design_bottom * scale;
    const float x0 = surface_left * 2.0f / static_cast<float>(g_surface_width) - 1.0f;
    const float x1 = surface_right * 2.0f / static_cast<float>(g_surface_width) - 1.0f;
    const float y0 = 1.0f - surface_top * 2.0f / static_cast<float>(g_surface_height);
    const float y1 = 1.0f - surface_bottom * 2.0f / static_cast<float>(g_surface_height);
    const GLfloat vertices[] = {x0, y0, x0, y1, x1, y0, x1, y1};
    bind_and_draw(asset.texture, vertices, true);
}

void draw() {
    if (offline_startup_flow::snapshot().route != offline_startup_flow::Route::kOpeningDialogue) {
        delete_texture(&g_background);
        for (auto& table : g_hero_tables) delete_texture(&table);
        return;
    }
    if (g_program == 0 || g_sampler < 0 ||
            g_surface_width <= 0 || g_surface_height <= 0) {
        return;
    }

    draw_background();
    draw_hero_table();
}

}  // namespace
}  // namespace nevergone::single_select_hero

extern "C" JNIEXPORT void JNICALL
Java_org_nevergone_recomp_GameSurfaceView_nativeOnSingleSelectHeroSurfaceCreated(JNIEnv*, jclass) {
    nevergone::single_select_hero::on_surface_created();
}

extern "C" JNIEXPORT void JNICALL
Java_org_nevergone_recomp_GameSurfaceView_nativeOnSingleSelectHeroSurfaceChanged(
        JNIEnv*, jclass, jint width, jint height) {
    nevergone::single_select_hero::on_surface_changed(
            static_cast<int>(width), static_cast<int>(height));
}

extern "C" JNIEXPORT void JNICALL
Java_org_nevergone_recomp_GameSurfaceView_nativeClearSingleSelectHeroBackground(JNIEnv*, jclass) {
    nevergone::single_select_hero::clear_background_asset();
}

extern "C" JNIEXPORT jboolean JNICALL
Java_org_nevergone_recomp_GameSurfaceView_nativeUploadSingleSelectHeroBackground(
        JNIEnv* env,
        jclass,
        jint width,
        jint height,
        jint left,
        jint top,
        jint source_width,
        jint source_height,
        jintArray pixels) {
    if (pixels == nullptr || width <= 0 || height <= 0) return JNI_FALSE;
    const jsize length = env->GetArrayLength(pixels);
    const size_t expected = static_cast<size_t>(width) * static_cast<size_t>(height);
    if (static_cast<size_t>(length) != expected) return JNI_FALSE;
    jint* values = env->GetIntArrayElements(pixels, nullptr);
    if (values == nullptr) return JNI_FALSE;
    const bool uploaded = nevergone::single_select_hero::upload_background(
            static_cast<int>(width),
            static_cast<int>(height),
            static_cast<int>(left),
            static_cast<int>(top),
            static_cast<int>(source_width),
            static_cast<int>(source_height),
            reinterpret_cast<const std::uint32_t*>(values),
            expected);
    env->ReleaseIntArrayElements(pixels, values, JNI_ABORT);
    return uploaded ? JNI_TRUE : JNI_FALSE;
}

extern "C" JNIEXPORT void JNICALL
Java_org_nevergone_recomp_SingleSelectHeroBaseComposer_nativeClearHeroTables(JNIEnv*, jclass) {
    nevergone::single_select_hero::clear_hero_tables();
}

extern "C" JNIEXPORT jboolean JNICALL
Java_org_nevergone_recomp_SingleSelectHeroBaseComposer_nativeUploadHeroTable(
        JNIEnv* env,
        jclass,
        jint table_index,
        jint width,
        jint height,
        jint left,
        jint top,
        jint source_width,
        jint source_height,
        jintArray pixels) {
    if (pixels == nullptr || width <= 0 || height <= 0) return JNI_FALSE;
    const jsize length = env->GetArrayLength(pixels);
    const size_t expected = static_cast<size_t>(width) * static_cast<size_t>(height);
    if (static_cast<size_t>(length) != expected) return JNI_FALSE;
    jint* values = env->GetIntArrayElements(pixels, nullptr);
    if (values == nullptr) return JNI_FALSE;
    const bool uploaded = nevergone::single_select_hero::upload_hero_table(
            static_cast<int>(table_index),
            static_cast<int>(width),
            static_cast<int>(height),
            static_cast<int>(left),
            static_cast<int>(top),
            static_cast<int>(source_width),
            static_cast<int>(source_height),
            reinterpret_cast<const std::uint32_t*>(values),
            expected);
    env->ReleaseIntArrayElements(pixels, values, JNI_ABORT);
    return uploaded ? JNI_TRUE : JNI_FALSE;
}

extern "C" JNIEXPORT void JNICALL
Java_org_nevergone_recomp_GameSurfaceView_nativeDrawSingleSelectHeroLayer(JNIEnv*, jclass) {
    nevergone::single_select_hero::draw();
}
