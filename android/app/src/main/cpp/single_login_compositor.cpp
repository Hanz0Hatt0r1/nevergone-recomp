#include <GLES2/gl2.h>
#include <jni.h>

#include <cstdint>
#include <vector>

#include "game_clock.h"
#include "splash_timeline.h"

namespace nevergone::single_login {
namespace {

GLuint g_program = 0;
GLint g_sampler = -1;
GLuint g_texture = 0;
int g_texture_width = 0;
int g_texture_height = 0;
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
void main() {
    gl_FragColor = texture2D(uTexture, vTexCoord);
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

void clear_texture() {
    if (g_texture != 0) {
        glDeleteTextures(1, &g_texture);
        g_texture = 0;
    }
    g_texture_width = 0;
    g_texture_height = 0;
}

void on_surface_created() {
    g_texture = 0;
    g_texture_width = 0;
    g_texture_height = 0;
    if (g_program != 0) glDeleteProgram(g_program);
    g_program = build_program();
    g_sampler = g_program != 0 ? glGetUniformLocation(g_program, "uTexture") : -1;
}

bool upload_texture(int width, int height, const std::uint32_t* argb, size_t count) {
    if (g_program == 0 || width <= 0 || height <= 0 || argb == nullptr ||
        count != static_cast<size_t>(width) * static_cast<size_t>(height) ||
        count > 16777216u) {
        return false;
    }
    std::vector<std::uint8_t> rgba(count * 4u);
    for (size_t i = 0; i < count; ++i) {
        const std::uint32_t pixel = argb[i];
        rgba[i * 4u + 0u] = static_cast<std::uint8_t>((pixel >> 16u) & 0xffu);
        rgba[i * 4u + 1u] = static_cast<std::uint8_t>((pixel >> 8u) & 0xffu);
        rgba[i * 4u + 2u] = static_cast<std::uint8_t>(pixel & 0xffu);
        rgba[i * 4u + 3u] = static_cast<std::uint8_t>((pixel >> 24u) & 0xffu);
    }

    clear_texture();
    glGenTextures(1, &g_texture);
    if (g_texture == 0) return false;
    glBindTexture(GL_TEXTURE_2D, g_texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, rgba.data());
    GLenum error = glGetError();
    glBindTexture(GL_TEXTURE_2D, 0);
    if (error != GL_NO_ERROR) {
        clear_texture();
        return false;
    }
    g_texture_width = width;
    g_texture_height = height;
    return true;
}

void draw() {
    if (g_texture == 0 || g_program == 0 || g_sampler < 0 ||
        g_surface_width <= 0 || g_surface_height <= 0) {
        return;
    }
    if (!splash_timeline::sample_tick(game_clock::tick_count()).complete) {
        return;
    }

    const float image_aspect = static_cast<float>(g_texture_width) / static_cast<float>(g_texture_height);
    const float surface_aspect = static_cast<float>(g_surface_width) / static_cast<float>(g_surface_height);
    float half_width = 1.0f;
    float half_height = 1.0f;
    if (image_aspect > surface_aspect) {
        half_height = surface_aspect / image_aspect;
    } else {
        half_width = image_aspect / surface_aspect;
    }
    const GLfloat vertices[] = {
        -half_width,  half_height,
        -half_width, -half_height,
         half_width,  half_height,
         half_width, -half_height,
    };
    static constexpr GLfloat texcoords[] = {
        0.0f, 0.0f,
        0.0f, 1.0f,
        1.0f, 0.0f,
        1.0f, 1.0f,
    };

    glClear(GL_COLOR_BUFFER_BIT);
    glUseProgram(g_program);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, g_texture);
    glUniform1i(g_sampler, 0);
    glEnableVertexAttribArray(0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, vertices);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 0, texcoords);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    glDisableVertexAttribArray(1);
    glDisableVertexAttribArray(0);
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

extern "C" JNIEXPORT void JNICALL
Java_org_nevergone_recomp_GameSurfaceView_nativeDrawSingleLoginLayer(JNIEnv*, jclass) {
    nevergone::single_login::draw();
}
