#include "single_select_hero_confirm_compositor.h"

#include <GLES2/gl2.h>
#include <jni.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

#include "character_name_state.h"
#include "offline_startup_flow.h"
#include "single_select_hero_confirm_input.h"
#include "single_select_hero_confirm_layout.h"
#include "single_select_hero_state.h"

namespace nevergone::single_select_hero_confirm_compositor {
namespace {

constexpr int kFrameCount = 4;
constexpr int kParentFrame = 0;   // btn_a.png
constexpr int kInnerFrame = 1;    // btn_b.png
constexpr int kNormalFrame = 2;   // btn_d.png
constexpr int kPressedFrame = 3;  // btn_e.png
constexpr std::size_t kMaxPixelsPerFrame = 16777216u;

struct FrameAsset {
    GLuint texture = 0;
    single_select_hero_confirm_layout::FrameGeometry geometry;
    std::vector<std::uint32_t> pixels;
};

GLuint g_program = 0;
GLint g_sampler = -1;
std::array<FrameAsset, kFrameCount> g_frames{};

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

bool ensure_program() {
    if (g_program != 0 && glIsProgram(g_program) == GL_TRUE && g_sampler >= 0) {
        return true;
    }
    g_program = build_program();
    g_sampler = g_program != 0 ? glGetUniformLocation(g_program, "uTexture") : -1;
    return g_program != 0 && g_sampler >= 0;
}

void delete_texture(FrameAsset* asset) {
    if (asset == nullptr) return;
    if (asset->texture != 0 && glIsTexture(asset->texture) == GL_TRUE) {
        glDeleteTextures(1, &asset->texture);
    }
    asset->texture = 0;
}

void clear_frames() {
    for (FrameAsset& asset : g_frames) {
        delete_texture(&asset);
        asset.geometry = {};
        asset.pixels.clear();
        asset.pixels.shrink_to_fit();
    }
}

GLuint create_texture(const std::vector<std::uint32_t>& argb, int width, int height) {
    const std::size_t expected =
        static_cast<std::size_t>(width) * static_cast<std::size_t>(height);
    if (width <= 0 || height <= 0 || argb.size() != expected ||
            expected > kMaxPixelsPerFrame) {
        return 0;
    }

    std::vector<std::uint8_t> rgba(expected * 4u);
    for (std::size_t index = 0; index < expected; ++index) {
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
    if (error == GL_NO_ERROR) return texture;
    glDeleteTextures(1, &texture);
    return 0;
}

bool ensure_texture(FrameAsset* asset) {
    if (asset == nullptr) return false;
    if (asset->texture != 0 && glIsTexture(asset->texture) == GL_TRUE) return true;
    asset->texture = create_texture(
        asset->pixels, asset->geometry.width, asset->geometry.height);
    return asset->texture != 0;
}

bool valid_geometry(
        int width,
        int height,
        int left,
        int top,
        int source_width,
        int source_height,
        std::size_t count) {
    return width > 0 && height > 0 && source_width > 0 && source_height > 0 &&
        left >= 0 && top >= 0 && left + width <= source_width &&
        top + height <= source_height &&
        count == static_cast<std::size_t>(width) * static_cast<std::size_t>(height) &&
        count <= kMaxPixelsPerFrame;
}

bool upload_frame(
        int frame_index,
        int width,
        int height,
        int left,
        int top,
        int source_width,
        int source_height,
        const std::uint32_t* pixels,
        std::size_t count) {
    if (frame_index < 0 || frame_index >= kFrameCount || pixels == nullptr ||
            !valid_geometry(
                width, height, left, top, source_width, source_height, count)) {
        return false;
    }

    FrameAsset& asset = g_frames[static_cast<std::size_t>(frame_index)];
    delete_texture(&asset);
    asset.geometry.width = width;
    asset.geometry.height = height;
    asset.geometry.left = left;
    asset.geometry.top = top;
    asset.geometry.source_width = source_width;
    asset.geometry.source_height = source_height;
    asset.pixels.assign(pixels, pixels + count);
    return true;
}

bool route_active() {
    return offline_startup_flow::snapshot().route ==
            offline_startup_flow::Route::kOpeningDialogue &&
        single_select_hero_state::snapshot().active &&
        !character_name_state::snapshot().active;
}

void draw_frame(int frame_index, int surface_width, int surface_height) {
    if (frame_index < 0 || frame_index >= kFrameCount) return;
    FrameAsset& asset = g_frames[static_cast<std::size_t>(frame_index)];
    if (!ensure_texture(&asset)) return;

    const auto quad = single_select_hero_confirm_layout::quad_for_surface(
        asset.geometry, surface_width, surface_height);
    if (!quad.valid) return;

    const GLfloat vertices[] = {
        quad.x0, quad.y0,
        quad.x0, quad.y1,
        quad.x1, quad.y0,
        quad.x1, quad.y1,
    };
    static constexpr GLfloat kTexCoords[] = {
        0.0f, 0.0f,
        0.0f, 1.0f,
        1.0f, 0.0f,
        1.0f, 1.0f,
    };

    glUseProgram(g_program);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, asset.texture);
    glUniform1i(g_sampler, 0);
    glEnableVertexAttribArray(0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, vertices);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 0, kTexCoords);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    glDisableVertexAttribArray(1);
    glDisableVertexAttribArray(0);
    glBindTexture(GL_TEXTURE_2D, 0);
}

}  // namespace

void draw() {
    if (!route_active() || !ensure_program()) return;

    GLint viewport[4] = {0, 0, 0, 0};
    glGetIntegerv(GL_VIEWPORT, viewport);
    if (viewport[2] <= 0 || viewport[3] <= 0) return;

    const GLboolean blend_was_enabled = glIsEnabled(GL_BLEND);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    // Recovered child order: btn_a parent background, then btn_b child, then
    // the btn_d/btn_e menu item. Equal child z-order preserves insertion order.
    draw_frame(kParentFrame, viewport[2], viewport[3]);
    draw_frame(kInnerFrame, viewport[2], viewport[3]);
    draw_frame(
        single_select_hero_confirm_input::pressed() ? kPressedFrame : kNormalFrame,
        viewport[2],
        viewport[3]);

    if (blend_was_enabled == GL_FALSE) glDisable(GL_BLEND);
}

}  // namespace nevergone::single_select_hero_confirm_compositor

extern "C" JNIEXPORT void JNICALL
Java_org_nevergone_recomp_SingleSelectHeroBaseComposer_nativeClearConfirmControl(
        JNIEnv*, jclass) {
    nevergone::single_select_hero_confirm_compositor::clear_frames();
}

extern "C" JNIEXPORT jboolean JNICALL
Java_org_nevergone_recomp_SingleSelectHeroBaseComposer_nativeUploadConfirmFrame(
        JNIEnv* env,
        jclass,
        jint frame_index,
        jint width,
        jint height,
        jint left,
        jint top,
        jint source_width,
        jint source_height,
        jintArray pixels) {
    if (pixels == nullptr || width <= 0 || height <= 0) return JNI_FALSE;
    const jsize length = env->GetArrayLength(pixels);
    const std::size_t expected =
        static_cast<std::size_t>(width) * static_cast<std::size_t>(height);
    if (static_cast<std::size_t>(length) != expected || expected > kMaxPixelsPerFrame) {
        return JNI_FALSE;
    }

    jint* values = env->GetIntArrayElements(pixels, nullptr);
    if (values == nullptr) return JNI_FALSE;
    const bool uploaded = nevergone::single_select_hero_confirm_compositor::upload_frame(
        static_cast<int>(frame_index),
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
