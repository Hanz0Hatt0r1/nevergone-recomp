#include <GLES2/gl2.h>
#include <jni.h>

#include <algorithm>
#include <cstdint>
#include <vector>

#include "game_clock.h"
#include "offline_startup_flow.h"

namespace nevergone::tutorial_loading_compositor {
namespace {

constexpr int kFrameCount = 3;
constexpr float kDesignWidth = 1136.0f;
constexpr float kDesignHeight = 640.0f;
constexpr std::uint64_t kTicksPerFrame = 6;

struct Frame {
    GLuint texture = 0;
    int width = 0;
    int height = 0;
    std::vector<std::uint32_t> pixels;
};

GLuint g_program = 0;
GLint g_sampler = -1;
Frame g_frames[kFrameCount]{};
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

void delete_texture(Frame* frame) {
    if (frame != nullptr && frame->texture != 0) {
        glDeleteTextures(1, &frame->texture);
        frame->texture = 0;
    }
}

GLuint create_texture(const Frame& frame) {
    if (frame.width <= 0 || frame.height <= 0 ||
        frame.pixels.size() != static_cast<size_t>(frame.width) *
                               static_cast<size_t>(frame.height)) {
        return 0;
    }

    std::vector<std::uint8_t> rgba(frame.pixels.size() * 4u);
    for (size_t index = 0; index < frame.pixels.size(); ++index) {
        const std::uint32_t pixel = frame.pixels[index];
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
        GL_TEXTURE_2D, 0, GL_RGBA, frame.width, frame.height, 0,
        GL_RGBA, GL_UNSIGNED_BYTE, rgba.data());
    const GLenum error = glGetError();
    glBindTexture(GL_TEXTURE_2D, 0);
    if (error != GL_NO_ERROR) {
        glDeleteTextures(1, &texture);
        return 0;
    }
    return texture;
}

bool ensure_texture(Frame* frame) {
    if (frame == nullptr) return false;
    if (frame->texture != 0) return true;
    frame->texture = create_texture(*frame);
    return frame->texture != 0;
}

int first_available_frame() {
    for (int index = 0; index < kFrameCount; ++index) {
        if (!g_frames[index].pixels.empty()) return index;
    }
    return -1;
}

int animated_frame() {
    int available[kFrameCount] = {};
    int count = 0;
    for (int index = 0; index < kFrameCount; ++index) {
        if (!g_frames[index].pixels.empty()) available[count++] = index;
    }
    if (count == 0) return -1;
    const std::uint64_t phase = game_clock::tick_count() / kTicksPerFrame;
    return available[phase % static_cast<std::uint64_t>(count)];
}

void draw_frame(Frame* frame) {
    if (frame == nullptr || !ensure_texture(frame) ||
        g_surface_width <= 0 || g_surface_height <= 0) {
        return;
    }

    // LoadingLayer resources are resolved through the original
    // gamescene_ui/loading_UI search path. Draw at their design-space native
    // size so both a full-screen 1136x640 frame and a trimmed centered logo
    // preserve the original Cocos content size semantics.
    const float scale = std::min(
        static_cast<float>(g_surface_width) / kDesignWidth,
        static_cast<float>(g_surface_height) / kDesignHeight);
    const float draw_width = static_cast<float>(frame->width) * scale;
    const float draw_height = static_cast<float>(frame->height) * scale;
    const float left = (static_cast<float>(g_surface_width) - draw_width) * 0.5f;
    const float top = (static_cast<float>(g_surface_height) - draw_height) * 0.5f;
    const float right = left + draw_width;
    const float bottom = top + draw_height;

    const GLfloat vertices[] = {
        left * 2.0f / g_surface_width - 1.0f,
        1.0f - top * 2.0f / g_surface_height,
        left * 2.0f / g_surface_width - 1.0f,
        1.0f - bottom * 2.0f / g_surface_height,
        right * 2.0f / g_surface_width - 1.0f,
        1.0f - top * 2.0f / g_surface_height,
        right * 2.0f / g_surface_width - 1.0f,
        1.0f - bottom * 2.0f / g_surface_height,
    };
    static constexpr GLfloat kTexCoords[] = {
        0.0f, 0.0f,
        0.0f, 1.0f,
        1.0f, 0.0f,
        1.0f, 1.0f,
    };

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glUseProgram(g_program);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, frame->texture);
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

}  // namespace

void on_surface_created() {
    for (auto& frame : g_frames) frame.texture = 0;
    if (g_program != 0) glDeleteProgram(g_program);
    g_program = build_program();
    g_sampler = g_program != 0 ? glGetUniformLocation(g_program, "uTexture") : -1;
}

void on_surface_changed(int width, int height) {
    g_surface_width = width;
    g_surface_height = height;
}

void clear_frames() {
    for (auto& frame : g_frames) {
        delete_texture(&frame);
        frame = {};
    }
}

bool upload_frame(
        int frame_index,
        int width,
        int height,
        const std::uint32_t* argb,
        size_t count) {
    if (frame_index < 0 || frame_index >= kFrameCount ||
        width <= 0 || height <= 0 || argb == nullptr ||
        count != static_cast<size_t>(width) * static_cast<size_t>(height) ||
        count > 16777216u) {
        return false;
    }
    Frame& frame = g_frames[frame_index];
    delete_texture(&frame);
    frame.width = width;
    frame.height = height;
    frame.pixels.assign(argb, argb + count);
    return true;
}

void draw() {
    if (offline_startup_flow::snapshot().route !=
        offline_startup_flow::Route::kTutorialLoading) {
        return;
    }

    // The frozen original loading capture has a black background. Clearing
    // here also guarantees that already-prepared GameScene geometry cannot
    // leak through while the tutorial loading boundary owns the surface.
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    if (g_program == 0 || g_sampler < 0) return;

    int index = animated_frame();
    if (index < 0) index = first_available_frame();
    if (index >= 0) draw_frame(&g_frames[index]);
}

}  // namespace nevergone::tutorial_loading_compositor

extern "C" JNIEXPORT void JNICALL
Java_org_nevergone_recomp_GameSurfaceView_nativeOnTutorialLoadingSurfaceCreated(
        JNIEnv*, jclass) {
    nevergone::tutorial_loading_compositor::on_surface_created();
}

extern "C" JNIEXPORT void JNICALL
Java_org_nevergone_recomp_GameSurfaceView_nativeOnTutorialLoadingSurfaceChanged(
        JNIEnv*, jclass, jint width, jint height) {
    nevergone::tutorial_loading_compositor::on_surface_changed(
        static_cast<int>(width), static_cast<int>(height));
}

extern "C" JNIEXPORT void JNICALL
Java_org_nevergone_recomp_GameSurfaceView_nativeClearTutorialLoadingFrames(
        JNIEnv*, jclass) {
    nevergone::tutorial_loading_compositor::clear_frames();
}

extern "C" JNIEXPORT jboolean JNICALL
Java_org_nevergone_recomp_GameSurfaceView_nativeUploadTutorialLoadingFrame(
        JNIEnv* env,
        jclass,
        jint frame_index,
        jint width,
        jint height,
        jintArray argb_pixels) {
    if (argb_pixels == nullptr) return JNI_FALSE;
    const jsize count = env->GetArrayLength(argb_pixels);
    if (count <= 0) return JNI_FALSE;
    jint* pixels = env->GetIntArrayElements(argb_pixels, nullptr);
    if (pixels == nullptr) return JNI_FALSE;
    const bool ok = nevergone::tutorial_loading_compositor::upload_frame(
        static_cast<int>(frame_index),
        static_cast<int>(width),
        static_cast<int>(height),
        reinterpret_cast<const std::uint32_t*>(pixels),
        static_cast<size_t>(count));
    env->ReleaseIntArrayElements(argb_pixels, pixels, JNI_ABORT);
    return ok ? JNI_TRUE : JNI_FALSE;
}

extern "C" JNIEXPORT void JNICALL
Java_org_nevergone_recomp_GameSurfaceView_nativeDrawTutorialLoadingLayer(
        JNIEnv*, jclass) {
    nevergone::tutorial_loading_compositor::draw();
}
