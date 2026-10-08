#include "render_bridge.h"

#include <GLES2/gl2.h>
#include <jni.h>

#include <atomic>
#include <cstdint>
#include <mutex>
#include <sstream>
#include <string>
#include <vector>

namespace nevergone::render {
namespace {

std::atomic<int> g_width{0};
std::atomic<int> g_height{0};
std::atomic<std::uint64_t> g_frame_count{0};
std::atomic<std::uint64_t> g_touch_count{0};
std::mutex g_touch_mutex;
int g_last_touch_action = -1;
int g_last_touch_pointer = -1;
float g_last_touch_x = 0.0f;
float g_last_touch_y = 0.0f;

std::mutex g_gl_status_mutex;
std::string g_gl_vendor;
std::string g_gl_renderer;
std::string g_gl_version;
std::string g_shader_status = "not initialized";
GLuint g_program = 0;

std::string gl_string(GLenum name) {
    const GLubyte* value = glGetString(name);
    return value != nullptr ? reinterpret_cast<const char*>(value) : "unavailable";
}

GLuint compile_shader(GLenum type, const char* source, std::string* error) {
    const GLuint shader = glCreateShader(type);
    if (shader == 0) {
        if (error != nullptr) *error = "glCreateShader returned 0";
        return 0;
    }

    glShaderSource(shader, 1, &source, nullptr);
    glCompileShader(shader);

    GLint compiled = GL_FALSE;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);
    if (compiled == GL_TRUE) {
        return shader;
    }

    GLint log_length = 0;
    glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &log_length);
    std::vector<char> log(static_cast<size_t>(log_length > 1 ? log_length : 1), '\0');
    if (log_length > 1) {
        glGetShaderInfoLog(shader, log_length, nullptr, log.data());
    }
    if (error != nullptr) {
        *error = log_length > 1 ? std::string(log.data()) : "shader compilation failed";
    }
    glDeleteShader(shader);
    return 0;
}

GLuint build_smoke_program(std::string* error) {
    static constexpr const char* kVertexShader = R"GLSL(
attribute vec2 aPosition;
void main() {
    gl_Position = vec4(aPosition, 0.0, 1.0);
}
)GLSL";

    static constexpr const char* kFragmentShader = R"GLSL(
precision mediump float;
void main() {
    gl_FragColor = vec4(0.72, 0.72, 0.72, 1.0);
}
)GLSL";

    std::string vertex_error;
    const GLuint vertex = compile_shader(GL_VERTEX_SHADER, kVertexShader, &vertex_error);
    if (vertex == 0) {
        if (error != nullptr) *error = "vertex shader: " + vertex_error;
        return 0;
    }

    std::string fragment_error;
    const GLuint fragment = compile_shader(GL_FRAGMENT_SHADER, kFragmentShader, &fragment_error);
    if (fragment == 0) {
        glDeleteShader(vertex);
        if (error != nullptr) *error = "fragment shader: " + fragment_error;
        return 0;
    }

    const GLuint program = glCreateProgram();
    if (program == 0) {
        glDeleteShader(vertex);
        glDeleteShader(fragment);
        if (error != nullptr) *error = "glCreateProgram returned 0";
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
    if (linked == GL_TRUE) {
        return program;
    }

    GLint log_length = 0;
    glGetProgramiv(program, GL_INFO_LOG_LENGTH, &log_length);
    std::vector<char> log(static_cast<size_t>(log_length > 1 ? log_length : 1), '\0');
    if (log_length > 1) {
        glGetProgramInfoLog(program, log_length, nullptr, log.data());
    }
    if (error != nullptr) {
        *error = log_length > 1 ? std::string(log.data()) : "program link failed";
    }
    glDeleteProgram(program);
    return 0;
}

void on_surface_created() {
    g_frame_count.store(0, std::memory_order_relaxed);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);

    std::string shader_error;
    g_program = build_smoke_program(&shader_error);

    std::lock_guard<std::mutex> lock(g_gl_status_mutex);
    g_gl_vendor = gl_string(GL_VENDOR);
    g_gl_renderer = gl_string(GL_RENDERER);
    g_gl_version = gl_string(GL_VERSION);
    g_shader_status = g_program != 0 ? "ok" : shader_error;
}

void on_surface_changed(int width, int height) {
    g_width.store(width, std::memory_order_relaxed);
    g_height.store(height, std::memory_order_relaxed);
    glViewport(0, 0, width, height);
}

void on_draw_frame() {
    glClear(GL_COLOR_BUFFER_BIT);

    if (g_program != 0) {
        static constexpr GLfloat kVertices[] = {
            0.0f, 0.55f,
            -0.48f, -0.45f,
            0.48f, -0.45f,
        };
        glUseProgram(g_program);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, kVertices);
        glDrawArrays(GL_TRIANGLES, 0, 3);
        glDisableVertexAttribArray(0);
    }

    g_frame_count.fetch_add(1, std::memory_order_relaxed);
}

void on_touch(int action, int pointer_id, float x, float y) {
    g_touch_count.fetch_add(1, std::memory_order_relaxed);
    std::lock_guard<std::mutex> lock(g_touch_mutex);
    g_last_touch_action = action;
    g_last_touch_pointer = pointer_id;
    g_last_touch_x = x;
    g_last_touch_y = y;
}

}  // namespace

std::string status_report() {
    std::ostringstream out;
    out << "render surface: " << g_width.load(std::memory_order_relaxed)
        << "x" << g_height.load(std::memory_order_relaxed) << "\n";
    out << "render frames: " << g_frame_count.load(std::memory_order_relaxed) << "\n";
    {
        std::lock_guard<std::mutex> lock(g_gl_status_mutex);
        out << "GL vendor: " << (g_gl_vendor.empty() ? "pending" : g_gl_vendor) << "\n";
        out << "GL renderer: " << (g_gl_renderer.empty() ? "pending" : g_gl_renderer) << "\n";
        out << "GL version: " << (g_gl_version.empty() ? "pending" : g_gl_version) << "\n";
        out << "shader pipeline: " << g_shader_status << "\n";
    }
    out << "touch events: " << g_touch_count.load(std::memory_order_relaxed) << "\n";
    {
        std::lock_guard<std::mutex> lock(g_touch_mutex);
        if (g_last_touch_action >= 0) {
            out << "last touch: action=" << g_last_touch_action
                << " pointer=" << g_last_touch_pointer
                << " x=" << g_last_touch_x
                << " y=" << g_last_touch_y << "\n";
        }
    }
    return out.str();
}

}  // namespace nevergone::render

extern "C" JNIEXPORT void JNICALL
Java_org_nevergone_recomp_GameSurfaceView_nativeOnSurfaceCreated(JNIEnv*, jclass) {
    nevergone::render::on_surface_created();
}

extern "C" JNIEXPORT void JNICALL
Java_org_nevergone_recomp_GameSurfaceView_nativeOnSurfaceChanged(
    JNIEnv*, jclass, jint width, jint height) {
    nevergone::render::on_surface_changed(static_cast<int>(width), static_cast<int>(height));
}

extern "C" JNIEXPORT void JNICALL
Java_org_nevergone_recomp_GameSurfaceView_nativeOnDrawFrame(JNIEnv*, jclass) {
    nevergone::render::on_draw_frame();
}

extern "C" JNIEXPORT void JNICALL
Java_org_nevergone_recomp_GameSurfaceView_nativeOnTouch(
    JNIEnv*, jclass, jint action, jint pointer_id, jfloat x, jfloat y) {
    nevergone::render::on_touch(
        static_cast<int>(action),
        static_cast<int>(pointer_id),
        static_cast<float>(x),
        static_cast<float>(y));
}
