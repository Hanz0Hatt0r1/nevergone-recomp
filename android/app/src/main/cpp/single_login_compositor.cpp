#include <GLES2/gl2.h>
#include <jni.h>

#include <array>
#include <cstdint>
#include <ctime>
#include <vector>

#include "game_clock.h"
#include "single_login_light_timeline.h"
#include "splash_sequence_state.h"

namespace nevergone::single_login {
namespace {

struct PositionedTexture {
    GLuint texture = 0;
    int width = 0;
    int height = 0;
    int left = 0;
    int top = 0;
    int source_width = 0;
    int source_height = 0;
};

constexpr size_t kBackgroundCount = 3;
constexpr size_t kBuildingCount = 5;

GLuint g_program = 0;
GLint g_sampler = -1;
GLint g_alpha = -1;
std::array<PositionedTexture, kBackgroundCount> g_backgrounds{};
std::array<PositionedTexture, kBuildingCount> g_buildings{};
std::array<PositionedTexture, single_login_light_timeline::kLightCount> g_lights{};
int g_surface_width = 0;
int g_surface_height = 0;
std::uint64_t g_scene_generation = 0;

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

template <size_t N>
void clear_layers(std::array<PositionedTexture, N>* layers) {
    if (layers == nullptr) return;
    for (auto& layer : *layers) {
        delete_texture(&layer.texture);
        layer = {};
    }
}

void clear_backgrounds() {
    clear_layers(&g_backgrounds);
}

void clear_buildings() {
    clear_layers(&g_buildings);
}

void clear_lights() {
    clear_layers(&g_lights);
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
    g_backgrounds = {};
    g_buildings = {};
    g_lights = {};
    g_scene_generation = 0;
    if (g_program != 0) glDeleteProgram(g_program);
    g_program = build_program();
    g_sampler = g_program != 0 ? glGetUniformLocation(g_program, "uTexture") : -1;
    g_alpha = g_program != 0 ? glGetUniformLocation(g_program, "uAlpha") : -1;
}

template <size_t N>
bool upload_positioned(
    std::array<PositionedTexture, N>* layers,
    int index,
    int width,
    int height,
    int left,
    int top,
    int source_width,
    int source_height,
    const std::uint32_t* argb,
    size_t count) {
    if (g_program == 0 || layers == nullptr || index < 0 || index >= static_cast<int>(layers->size()) ||
        width <= 0 || height <= 0 || source_width <= 0 || source_height <= 0 || left < 0 || top < 0 ||
        left + width > source_width || top + height > source_height) {
        return false;
    }
    GLuint texture = create_texture(argb, count, width, height);
    if (texture == 0) return false;

    auto& layer = (*layers)[static_cast<size_t>(index)];
    delete_texture(&layer.texture);
    layer.texture = texture;
    layer.width = width;
    layer.height = height;
    layer.left = left;
    layer.top = top;
    layer.source_width = source_width;
    layer.source_height = source_height;
    return true;
}

bool upload_background(
    int index,
    int width,
    int height,
    int left,
    int top,
    int source_width,
    int source_height,
    const std::uint32_t* argb,
    size_t count) {
    return upload_positioned(
        &g_backgrounds, index, width, height, left, top, source_width, source_height, argb, count);
}

bool upload_building(
    int index,
    int width,
    int height,
    int left,
    int top,
    int source_width,
    int source_height,
    const std::uint32_t* argb,
    size_t count) {
    return upload_positioned(
        &g_buildings, index, width, height, left, top, source_width, source_height, argb, count);
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
    return upload_positioned(
        &g_lights, index, width, height, left, top, source_width, source_height, argb, count);
}

bool backgrounds_ready() {
    for (const auto& layer : g_backgrounds) {
        if (layer.texture == 0) return false;
    }
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

void draw_positioned(const PositionedTexture& layer, float alpha) {
    if (layer.texture == 0 || alpha <= 0.0f || layer.source_width <= 0 || layer.source_height <= 0) {
        return;
    }

    float half_width = 1.0f;
    float half_height = 1.0f;
    scene_half_extents(layer.source_width, layer.source_height, &half_width, &half_height);
    const float x0 = -half_width +
        2.0f * half_width * static_cast<float>(layer.left) / static_cast<float>(layer.source_width);
    const float x1 = -half_width +
        2.0f * half_width * static_cast<float>(layer.left + layer.width) /
            static_cast<float>(layer.source_width);
    const float y0 = half_height -
        2.0f * half_height * static_cast<float>(layer.top) / static_cast<float>(layer.source_height);
    const float y1 = half_height -
        2.0f * half_height * static_cast<float>(layer.top + layer.height) /
            static_cast<float>(layer.source_height);
    const GLfloat vertices[] = {
        x0, y0,
        x0, y1,
        x1, y0,
        x1, y1,
    };
    draw_quad(layer.texture, vertices, alpha);
}

void draw() {
    if (!backgrounds_ready() || g_program == 0 || g_sampler < 0 || g_alpha < 0 ||
        g_surface_width <= 0 || g_surface_height <= 0) {
        return;
    }

    const std::uint64_t tick = game_clock::tick_count();
    const double scene_seconds = splash_sequence_state::single_login_seconds(tick);
    if (scene_seconds < 0.0) return;

    const std::uint64_t generation = splash_sequence_state::generation();
    if (generation != g_scene_generation) {
        const std::time_t now = std::time(nullptr);
        single_login_light_timeline::reset(now > 0 ? static_cast<std::uint64_t>(now) : 0ULL);
        g_scene_generation = generation;
    }

    glClear(GL_COLOR_BUFFER_BIT);
    glUseProgram(g_program);

    // Recovered static stack. These are kept as distinct draw calls so the
    // confirmed cloud layers can later be inserted at z=1/2/3 without
    // flattening them above the entire background.
    draw_positioned(g_backgrounds[0], 1.0f);  // z=0: zjmbeijing.png
    draw_positioned(g_backgrounds[1], 1.0f);  // z=2: zjmbeijing02.png
    draw_positioned(g_backgrounds[2], 1.0f);  // z=3: zjmbeijing03.png

    for (const auto& building : g_buildings) {
        draw_positioned(building, 1.0f);       // z=4
    }

    const auto light_sample = single_login_light_timeline::sample(scene_seconds);
    for (size_t index = 0; index < g_lights.size(); ++index) {
        draw_positioned(g_lights[index], light_sample.alpha[index]);  // z=5
    }

    glBindTexture(GL_TEXTURE_2D, 0);
}

bool upload_jni_layer(
    JNIEnv* env,
    jint index,
    jint width,
    jint height,
    jint left,
    jint top,
    jint source_width,
    jint source_height,
    jintArray pixels,
    bool (*uploader)(int, int, int, int, int, int, int, const std::uint32_t*, size_t)) {
    if (pixels == nullptr || width <= 0 || height <= 0 || uploader == nullptr) return false;
    const jsize length = env->GetArrayLength(pixels);
    const size_t expected = static_cast<size_t>(width) * static_cast<size_t>(height);
    if (static_cast<size_t>(length) != expected) return false;
    jint* values = env->GetIntArrayElements(pixels, nullptr);
    if (values == nullptr) return false;
    const bool ok = uploader(
        static_cast<int>(index),
        static_cast<int>(width),
        static_cast<int>(height),
        static_cast<int>(left),
        static_cast<int>(top),
        static_cast<int>(source_width),
        static_cast<int>(source_height),
        reinterpret_cast<const std::uint32_t*>(values),
        expected);
    env->ReleaseIntArrayElements(pixels, values, JNI_ABORT);
    return ok;
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
Java_org_nevergone_recomp_GameSurfaceView_nativeClearSingleLoginBackgrounds(JNIEnv*, jclass) {
    nevergone::single_login::clear_backgrounds();
}

extern "C" JNIEXPORT void JNICALL
Java_org_nevergone_recomp_GameSurfaceView_nativeClearSingleLoginBuildings(JNIEnv*, jclass) {
    nevergone::single_login::clear_buildings();
}

extern "C" JNIEXPORT void JNICALL
Java_org_nevergone_recomp_GameSurfaceView_nativeClearSingleLoginLights(JNIEnv*, jclass) {
    nevergone::single_login::clear_lights();
}

extern "C" JNIEXPORT jboolean JNICALL
Java_org_nevergone_recomp_GameSurfaceView_nativeUploadSingleLoginBackground(
    JNIEnv* env,
    jclass,
    jint background_index,
    jint width,
    jint height,
    jint left,
    jint top,
    jint source_width,
    jint source_height,
    jintArray pixels) {
    return nevergone::single_login::upload_jni_layer(
        env, background_index, width, height, left, top, source_width, source_height, pixels,
        nevergone::single_login::upload_background) ? JNI_TRUE : JNI_FALSE;
}

extern "C" JNIEXPORT jboolean JNICALL
Java_org_nevergone_recomp_GameSurfaceView_nativeUploadSingleLoginBuilding(
    JNIEnv* env,
    jclass,
    jint building_index,
    jint width,
    jint height,
    jint left,
    jint top,
    jint source_width,
    jint source_height,
    jintArray pixels) {
    return nevergone::single_login::upload_jni_layer(
        env, building_index, width, height, left, top, source_width, source_height, pixels,
        nevergone::single_login::upload_building) ? JNI_TRUE : JNI_FALSE;
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
    return nevergone::single_login::upload_jni_layer(
        env, light_index, width, height, left, top, source_width, source_height, pixels,
        nevergone::single_login::upload_light) ? JNI_TRUE : JNI_FALSE;
}

extern "C" JNIEXPORT void JNICALL
Java_org_nevergone_recomp_GameSurfaceView_nativeDrawSingleLoginLayer(JNIEnv*, jclass) {
    nevergone::single_login::draw();
}
