#include <GLES2/gl2.h>
#include <jni.h>

#include <array>
#include <cstdint>
#include <ctime>
#include <vector>

#include "game_clock.h"
#include "single_login_light_timeline.h"
#include "splash_timeline.h"

namespace nevergone::single_login {
namespace {

struct LightTexture {
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
GLint g_alpha = -1;
GLuint g_texture = 0;
int g_texture_width = 0;
int g_texture_height = 0;
std::array<LightTexture, single_login_light_timeline::kLightCount> g_lights{};
int g_surface_width = 0;
int g_surface_height = 0;

GLuint compile_shader(GLenum type, const char* source) {
    GLuint shader = glCreateShader(type);
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

    GLuint vertex = compile_shader(GL_VERTEX_SHADER, kVertex);
    GLuint fragment = compile_shader(GL_FRAGMENT_SHADER, kFragment);
    if (vertex == 0 || fragment == 0) {
        if (vertex != 0) glDeleteShader(vertex);
        if (fragment != 0) glDeleteShader(fragment);
        return 0;
    }
    GLuint program = glCreateProgram();
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
    if (texture != nullptr && *texture != 0) {
        glDeleteTextures(1, texture);
        *texture = 0;
    }
}

void clear_texture() {
    delete_texture(&g_texture);
    g_texture_width = 0;
    g_texture_height = 0;
}

void clear_lights() {
    for (auto& light : g_lights) {
        delete_texture(&light.texture);
        light = {};
    }
}

GLuint create_texture(const std::uint32_t* argb, size_t count, int width, int height) {
    if (width <= 0 || height <= 0 || argb == nullptr ||
        count != static_cast<size_t>(width) * static_cast<size_t>(height) ||
        count > 16777216u) {
        return 0;
    }

    std::vector<std::uint8_t> rgba(count * 4u);
    for (size_t i = 0; i < count; ++i) {
        const std::uint32_t pixel = argb[i];
        rgba[i * 4u + 0u] = static_cast<std::uint8_t>((pixel >> 16u) & 0xffu);
        rgba[i * 4u + 1u] = static_cast<std::uint8_t>((pixel >> 8u) & 0xffu);
        rgba[i * 4u + 2u] = static_cast<std::uint8_t>(pixel & 0xffu);
        rgba[i * 4u + 3u] = static_cast<std::uint8_t>((pixel >> 24u) & 0xffu);
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
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, rgba.data());
    const GLenum error = glGetError();
    glBindTexture(GL_TEXTURE_2D, 0);
    if (error != GL_NO_ERROR) {
        glDeleteTextures(1, &texture);
        return 0;
    }
    return texture;
}

void on_surface_created() {
    // A recreated GL context invalidates old object names; do not attempt to
    // delete them from the new context.
    g_texture = 0;
    g_texture_width = 0;
    g_texture_height = 0;
    g_lights = {};
    if (g_program != 0) glDeleteProgram(g_program);
    g_program = build_program();
    g_sampler = g_program != 0 ? glGetUniformLocation(g_program, "uTexture") : -1;
    g_alpha = g_program != 0 ? glGetUniformLocation(g_program, "uAlpha") : -1;

    const std::time_t now = std::time(nullptr);
    single_login_light_timeline::reset(now > 0 ? static_cast<std::uint64_t>(now) : 0ULL);
}

bool upload_texture(int width, int height, const std::uint32_t* argb, size_t count) {
    if (g_program == 0) return false;
    GLuint texture = create_texture(argb, count, width, height);
    if (texture == 0) return false;

    clear_texture();
    g_texture = texture;
    g_texture_width = width;
    g_texture_height = height;
    return true;
}

bool upload_light(
    int index,
    int width,
    int height,
    int left,
    int top,
    int source_width,
    int source_height,
    const std::uint32_t* argb,
    size_t count) {
    if (g_program == 0 || index < 0 || index >= static_cast<int>(g_lights.size()) ||
        source_width <= 0 || source_height <= 0 || left < 0 || top < 0 ||
        left + width > source_width || top + height > source_height) {
        return false;
    }
    GLuint texture = create_texture(argb, count, width, height);
    if (texture == 0) return false;

    auto& light = g_lights[static_cast<size_t>(index)];
    delete_texture(&light.texture);
    light.texture = texture;
    light.width = width;
    light.height = height;
    light.left = left;
    light.top = top;
    light.source_width = source_width;
    light.source_height = source_height;
    return true;
}

void draw_quad(GLuint texture, const GLfloat* vertices, float alpha) {
    static constexpr GLfloat kTexcoords[] = {
        0.0f, 0.0f,
        0.0f, 1.0f,
        1.0f, 0.0f,
        1.0f, 1.0f,
    };
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, texture);
    glUniform1i(g_sampler, 0);
    glUniform1f(g_alpha, alpha);
    glEnableVertexAttribArray(0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, vertices);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 0, kTexcoords);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    glDisableVertexAttribArray(1);
    glDisableVertexAttribArray(0);
}

void scene_half_extents(int source_width, int source_height, float* half_width, float* half_height) {
    const float image_aspect = static_cast<float>(source_width) / static_cast<float>(source_height);
    const float surface_aspect = static_cast<float>(g_surface_width) / static_cast<float>(g_surface_height);
    *half_width = 1.0f;
    *half_height = 1.0f;
    if (image_aspect > surface_aspect) {
        *half_height = surface_aspect / image_aspect;
    } else {
        *half_width = image_aspect / surface_aspect;
    }
}

void draw() {
    if (g_texture == 0 || g_program == 0 || g_sampler < 0 || g_alpha < 0 ||
        g_surface_width <= 0 || g_surface_height <= 0) {
        return;
    }

    const std::uint64_t tick = game_clock::tick_count();
    if (!splash_timeline::sample_tick(tick).complete) return;

    float half_width = 1.0f;
    float half_height = 1.0f;
    scene_half_extents(g_texture_width, g_texture_height, &half_width, &half_height);
    const GLfloat base_vertices[] = {
        -half_width,  half_height,
        -half_width, -half_height,
         half_width,  half_height,
         half_width, -half_height,
    };

    glClear(GL_COLOR_BUFFER_BIT);
    glUseProgram(g_program);
    draw_quad(g_texture, base_vertices, 1.0f);

    const double scene_seconds = static_cast<double>(tick) * game_clock::kFixedStepSeconds -
        splash_timeline::kTimelineCompleteSeconds;
    const auto light_sample = single_login_light_timeline::sample(scene_seconds);

    for (size_t index = 0; index < g_lights.size(); ++index) {
        const auto& light = g_lights[index];
        const float alpha = light_sample.alpha[index];
        if (light.texture == 0 || alpha <= 0.0f ||
            light.source_width <= 0 || light.source_height <= 0) {
            continue;
        }

        float light_half_width = 1.0f;
        float light_half_height = 1.0f;
        scene_half_extents(light.source_width, light.source_height, &light_half_width, &light_half_height);
        const float x0 = -light_half_width +
            2.0f * light_half_width * static_cast<float>(light.left) / static_cast<float>(light.source_width);
        const float x1 = -light_half_width +
            2.0f * light_half_width * static_cast<float>(light.left + light.width) /
                static_cast<float>(light.source_width);
        const float y0 = light_half_height -
            2.0f * light_half_height * static_cast<float>(light.top) / static_cast<float>(light.source_height);
        const float y1 = light_half_height -
            2.0f * light_half_height * static_cast<float>(light.top + light.height) /
                static_cast<float>(light.source_height);
        const GLfloat vertices[] = {
            x0, y0,
            x0, y1,
            x1, y0,
            x1, y1,
        };
        draw_quad(light.texture, vertices, alpha);
    }

    glBindTexture(GL_TEXTURE_2D, 0);
}

}  // namespace
}  // namespace nevergone::single_login

extern "C" JNIEXPORT void JNICALL
Java_org_nevergone_recomp_GameSurfaceView_nativeOnSingleLoginSurfaceCreated(JNIEnv*, jclass) {
    nevergone::single_login::on_surface_created();
}

extern "C" JNIEXPORT void JNICALL
Java_org_nevergone_recomp_GameSurfaceView_nativeOnSingleLoginSurfaceChanged(
    JNIEnv*, jclass, jint width, jint height) {
    nevergone::single_login::g_surface_width = static_cast<int>(width);
    nevergone::single_login::g_surface_height = static_cast<int>(height);
}

extern "C" JNIEXPORT void JNICALL
Java_org_nevergone_recomp_GameSurfaceView_nativeClearSingleLoginTexture(JNIEnv*, jclass) {
    nevergone::single_login::clear_texture();
}

extern "C" JNIEXPORT void JNICALL
Java_org_nevergone_recomp_GameSurfaceView_nativeClearSingleLoginLights(JNIEnv*, jclass) {
    nevergone::single_login::clear_lights();
}

extern "C" JNIEXPORT jboolean JNICALL
Java_org_nevergone_recomp_GameSurfaceView_nativeUploadSingleLoginTexture(
    JNIEnv* env, jclass, jint width, jint height, jintArray pixels) {
    if (pixels == nullptr || width <= 0 || height <= 0) return JNI_FALSE;
    const jsize length = env->GetArrayLength(pixels);
    const size_t expected = static_cast<size_t>(width) * static_cast<size_t>(height);
    if (static_cast<size_t>(length) != expected) return JNI_FALSE;
    jint* values = env->GetIntArrayElements(pixels, nullptr);
    if (values == nullptr) return JNI_FALSE;
    const bool ok = nevergone::single_login::upload_texture(
        static_cast<int>(width), static_cast<int>(height),
        reinterpret_cast<const std::uint32_t*>(values), expected);
    env->ReleaseIntArrayElements(pixels, values, JNI_ABORT);
    return ok ? JNI_TRUE : JNI_FALSE;
}

extern "C" JNIEXPORT jboolean JNICALL
Java_org_nevergone_recomp_GameSurfaceView_nativeUploadSingleLoginLight(
    JNIEnv* env,
    jclass,
    jint light_index,
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
    const bool ok = nevergone::single_login::upload_light(
        static_cast<int>(light_index),
        static_cast<int>(width),
        static_cast<int>(height),
        static_cast<int>(left),
        static_cast<int>(top),
        static_cast<int>(source_width),
        static_cast<int>(source_height),
        reinterpret_cast<const std::uint32_t*>(values),
        expected);
    env->ReleaseIntArrayElements(pixels, values, JNI_ABORT);
    return ok ? JNI_TRUE : JNI_FALSE;
}

extern "C" JNIEXPORT void JNICALL
Java_org_nevergone_recomp_GameSurfaceView_nativeDrawSingleLoginLayer(JNIEnv*, jclass) {
    nevergone::single_login::draw();
}
