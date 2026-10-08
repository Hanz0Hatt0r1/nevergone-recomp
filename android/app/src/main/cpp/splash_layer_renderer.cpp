#include <GLES2/gl2.h>
#include <jni.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

#include "game_clock.h"
#include "splash_timeline.h"

namespace nevergone::splash_layer_renderer {
namespace {

constexpr std::size_t kFrameCount = 6;
constexpr std::size_t kMaxPixelsPerFrame = 16777216u;

GLuint g_program = 0;
GLint g_sampler_uniform = -1;
GLint g_alpha_uniform = -1;
std::array<GLuint, kFrameCount> g_textures{};
std::array<int, kFrameCount> g_widths{};
std::array<int, kFrameCount> g_heights{};
GLuint g_black_texture = 0;
bool g_active = false;
std::uint64_t g_start_tick = 0;

GLuint compile_shader(GLenum type, const char* source) {
    const GLuint shader = glCreateShader(type);
    if (shader == 0) return 0;
    glShaderSource(shader, 1, &source, nullptr);
    glCompileShader(shader);
    GLint compiled = GL_FALSE;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);
    if (compiled != GL_TRUE) {
        glDeleteShader(shader);
        return 0;
    }
    return shader;
}

GLuint build_program() {
    static constexpr const char* kVertexShader = R"GLSL(
attribute vec2 aPosition;
attribute vec2 aTexCoord;
varying vec2 vTexCoord;
void main() {
    vTexCoord = aTexCoord;
    gl_Position = vec4(aPosition, 0.0, 1.0);
}
)GLSL";

    static constexpr const char* kFragmentShader = R"GLSL(
precision mediump float;
varying vec2 vTexCoord;
uniform sampler2D uTexture;
uniform float uAlpha;
void main() {
    vec4 color = texture2D(uTexture, vTexCoord);
    gl_FragColor = vec4(color.rgb, color.a * uAlpha);
}
)GLSL";

    const GLuint vertex = compile_shader(GL_VERTEX_SHADER, kVertexShader);
    const GLuint fragment = compile_shader(GL_FRAGMENT_SHADER, kFragmentShader);
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

void delete_texture(GLuint* texture) {
    if (*texture != 0) {
        glDeleteTextures(1, texture);
        *texture = 0;
    }
}

void clear_frames() {
    for (std::size_t i = 0; i < kFrameCount; ++i) {
        delete_texture(&g_textures[i]);
        g_widths[i] = 0;
        g_heights[i] = 0;
    }
    g_active = false;
}

bool upload_frame(
    int frame_index,
    int width,
    int height,
    const std::uint32_t* argb,
    std::size_t count) {
    if (frame_index < 0 || frame_index >= static_cast<int>(kFrameCount) ||
        width <= 0 || height <= 0 || argb == nullptr ||
        count != static_cast<std::size_t>(width) * static_cast<std::size_t>(height) ||
        count > kMaxPixelsPerFrame || g_program == 0) {
        return false;
    }

    std::vector<std::uint8_t> rgba(count * 4u);
    for (std::size_t i = 0; i < count; ++i) {
        const std::uint32_t pixel = argb[i];
        rgba[i * 4u + 0u] = static_cast<std::uint8_t>((pixel >> 16u) & 0xffu);
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
    glTexImage2D(
        GL_TEXTURE_2D,
        0,
        GL_RGBA,
        width,
        height,
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

    const std::size_t index = static_cast<std::size_t>(frame_index);
    delete_texture(&g_textures[index]);
    g_textures[index] = texture;
    g_widths[index] = width;
    g_heights[index] = height;
    return true;
}

bool all_frames_ready() {
    for (std::size_t i = 0; i < kFrameCount; ++i) {
        if (g_textures[i] == 0 || g_widths[i] <= 0 || g_heights[i] <= 0) return false;
    }
    return true;
}

void draw_texture(GLuint texture, int width, int height, float alpha, bool full_screen) {
    if (texture == 0 || alpha <= 0.0f || g_program == 0) return;

    GLint viewport[4] = {0, 0, 0, 0};
    glGetIntegerv(GL_VIEWPORT, viewport);
    if (viewport[2] <= 0 || viewport[3] <= 0) return;

    GLfloat half_width = 1.0f;
    GLfloat half_height = 1.0f;
    if (!full_screen && width > 0 && height > 0) {
        const GLfloat image_aspect = static_cast<GLfloat>(width) / static_cast<GLfloat>(height);
        const GLfloat surface_aspect = static_cast<GLfloat>(viewport[2]) / static_cast<GLfloat>(viewport[3]);
        half_width = 0.90f;
        half_height = 0.90f;
        if (image_aspect > surface_aspect) {
            half_height = half_width * surface_aspect / image_aspect;
        } else {
            half_width = half_height * image_aspect / surface_aspect;
        }
    }

    const GLfloat vertices[] = {
        -half_width,  half_height,
        -half_width, -half_height,
         half_width,  half_height,
         half_width, -half_height,
    };
    static constexpr GLfloat kTexCoords[] = {
        0.0f, 0.0f,
        0.0f, 1.0f,
        1.0f, 0.0f,
        1.0f, 1.0f,
    };

    glUseProgram(g_program);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, texture);
    glUniform1i(g_sampler_uniform, 0);
    glUniform1f(g_alpha_uniform, alpha);
    glEnableVertexAttribArray(0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, vertices);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 0, kTexCoords);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    glDisableVertexAttribArray(1);
    glDisableVertexAttribArray(0);
    glBindTexture(GL_TEXTURE_2D, 0);
}

void on_surface_created() {
    // A recreated GL context invalidates every old object name; reset without
    // deleting names that belong to the destroyed context.
    g_program = build_program();
    g_sampler_uniform = g_program != 0 ? glGetUniformLocation(g_program, "uTexture") : -1;
    g_alpha_uniform = g_program != 0 ? glGetUniformLocation(g_program, "uAlpha") : -1;
    g_textures.fill(0);
    g_widths.fill(0);
    g_heights.fill(0);
    g_black_texture = 0;
    g_active = false;

    if (g_program == 0 || g_sampler_uniform < 0 || g_alpha_uniform < 0) return;

    const std::uint8_t black_pixel[4] = {0, 0, 0, 255};
    glGenTextures(1, &g_black_texture);
    if (g_black_texture == 0) return;
    glBindTexture(GL_TEXTURE_2D, g_black_texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage2D(
        GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, black_pixel);
    glBindTexture(GL_TEXTURE_2D, 0);
}

bool begin_sequence() {
    if (g_program == 0 || g_black_texture == 0 || !all_frames_ready()) return false;
    g_start_tick = nevergone::game_clock::tick_count();
    g_active = true;
    return true;
}

void draw() {
    if (!g_active) return;

    const std::uint64_t now = nevergone::game_clock::tick_count();
    const std::uint64_t elapsed_tick = now >= g_start_tick ? now - g_start_tick : 0;
    const auto timeline = nevergone::splash_timeline::sample_tick(elapsed_tick);
    if (timeline.complete) {
        g_active = false;
        return;
    }

    // Replace the existing diagnostic renderer while the verified original
    // splash sequence is active. When this compositor becomes inactive, the
    // existing renderer is visible again without sharing GL ownership.
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    for (std::size_t i = 0; i < kFrameCount; ++i) {
        draw_texture(g_textures[i], g_widths[i], g_heights[i], timeline.frame_alpha[i], false);
    }
    draw_texture(g_black_texture, 1, 1, timeline.black_overlay_alpha, true);
}

}  // namespace
}  // namespace nevergone::splash_layer_renderer

extern "C" JNIEXPORT void JNICALL
Java_org_nevergone_recomp_GameSurfaceView_nativeOnSplashSurfaceCreated(JNIEnv*, jclass) {
    nevergone::splash_layer_renderer::on_surface_created();
}

extern "C" JNIEXPORT void JNICALL
Java_org_nevergone_recomp_GameSurfaceView_nativeClearSplashFrames(JNIEnv*, jclass) {
    nevergone::splash_layer_renderer::clear_frames();
}

extern "C" JNIEXPORT jboolean JNICALL
Java_org_nevergone_recomp_GameSurfaceView_nativeUploadSplashFrame(
    JNIEnv* env,
    jclass,
    jint frame_index,
    jint width,
    jint height,
    jintArray pixels) {
    if (pixels == nullptr || width <= 0 || height <= 0) return JNI_FALSE;
    const jsize length = env->GetArrayLength(pixels);
    const std::size_t expected = static_cast<std::size_t>(width) * static_cast<std::size_t>(height);
    if (static_cast<std::size_t>(length) != expected || expected > 16777216u) return JNI_FALSE;

    jint* values = env->GetIntArrayElements(pixels, nullptr);
    if (values == nullptr) return JNI_FALSE;
    const bool uploaded = nevergone::splash_layer_renderer::upload_frame(
        static_cast<int>(frame_index),
        static_cast<int>(width),
        static_cast<int>(height),
        reinterpret_cast<const std::uint32_t*>(values),
        expected);
    env->ReleaseIntArrayElements(pixels, values, JNI_ABORT);
    return uploaded ? JNI_TRUE : JNI_FALSE;
}

extern "C" JNIEXPORT jboolean JNICALL
Java_org_nevergone_recomp_GameSurfaceView_nativeBeginSplashSequence(JNIEnv*, jclass) {
    return nevergone::splash_layer_renderer::begin_sequence() ? JNI_TRUE : JNI_FALSE;
}

extern "C" JNIEXPORT void JNICALL
Java_org_nevergone_recomp_GameSurfaceView_nativeDrawSplashLayers(JNIEnv*, jclass) {
    nevergone::splash_layer_renderer::draw();
}
