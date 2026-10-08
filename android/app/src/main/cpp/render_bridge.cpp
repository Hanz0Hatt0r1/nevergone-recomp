#include "render_bridge.h"

#include <GLES2/gl2.h>
#include <jni.h>

#include <atomic>
#include <cstdint>
#include <mutex>
#include <sstream>
#include <string>
#include <vector>

#include "client_callback_bridge.h"
#include "game_clock.h"

namespace nevergone::render {
namespace {

struct RenderPhase {
    const char* name;
    GLfloat red;
    GLfloat green;
    GLfloat blue;
};

std::atomic<int> g_width{0};
std::atomic<int> g_height{0};
std::atomic<std::uint64_t> g_frame_count{0};
std::atomic<std::uint64_t> g_touch_count{0};
std::atomic<bool> g_app_resumed{false};
std::atomic<std::uint64_t> g_pause_count{0};
std::atomic<std::uint64_t> g_resume_count{0};
std::atomic<std::uint64_t> g_surface_generation{0};
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
std::string g_splash_status = "not loaded";
GLuint g_program = 0;
GLint g_color_uniform = -1;
GLuint g_splash_program = 0;
GLint g_splash_sampler_uniform = -1;
GLuint g_splash_texture = 0;
int g_splash_width = 0;
int g_splash_height = 0;

RenderPhase current_render_phase() {
    const auto state = nevergone::lua_runtime::snapshot_client_ui_state();
    if (!state.enter_game.empty()) return {"entered-game", 0.20f, 0.78f, 0.36f};
    if (!state.pve_connect.empty()) return {"pve", 0.86f, 0.30f, 0.25f};
    if (!state.update_data.empty()) return {"data", 0.30f, 0.70f, 0.86f};
    if (!state.chat_messages.empty()) return {"chat", 0.72f, 0.42f, 0.86f};
    if (!state.created_role.empty() || !state.role_list.empty()) {
        return {"role", 0.94f, 0.67f, 0.24f};
    }
    if (!state.server_list.empty()) return {"server", 0.24f, 0.52f, 0.90f};
    if (!state.announcement.empty()) return {"announcement", 0.88f, 0.78f, 0.26f};
    return {"idle", 0.72f, 0.72f, 0.72f};
}

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
    if (compiled == GL_TRUE) return shader;

    GLint log_length = 0;
    glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &log_length);
    std::vector<char> log(static_cast<size_t>(log_length > 1 ? log_length : 1), '\0');
    if (log_length > 1) glGetShaderInfoLog(shader, log_length, nullptr, log.data());
    if (error != nullptr) {
        *error = log_length > 1 ? std::string(log.data()) : "shader compilation failed";
    }
    glDeleteShader(shader);
    return 0;
}

GLuint link_program(
    const char* vertex_source,
    const char* fragment_source,
    const std::vector<std::pair<GLuint, const char*>>& attributes,
    std::string* error) {
    std::string vertex_error;
    const GLuint vertex = compile_shader(GL_VERTEX_SHADER, vertex_source, &vertex_error);
    if (vertex == 0) {
        if (error != nullptr) *error = "vertex shader: " + vertex_error;
        return 0;
    }

    std::string fragment_error;
    const GLuint fragment = compile_shader(GL_FRAGMENT_SHADER, fragment_source, &fragment_error);
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
    for (const auto& attribute : attributes) {
        glBindAttribLocation(program, attribute.first, attribute.second);
    }
    glLinkProgram(program);
    glDeleteShader(vertex);
    glDeleteShader(fragment);

    GLint linked = GL_FALSE;
    glGetProgramiv(program, GL_LINK_STATUS, &linked);
    if (linked == GL_TRUE) return program;

    GLint log_length = 0;
    glGetProgramiv(program, GL_INFO_LOG_LENGTH, &log_length);
    std::vector<char> log(static_cast<size_t>(log_length > 1 ? log_length : 1), '\0');
    if (log_length > 1) glGetProgramInfoLog(program, log_length, nullptr, log.data());
    if (error != nullptr) {
        *error = log_length > 1 ? std::string(log.data()) : "program link failed";
    }
    glDeleteProgram(program);
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
uniform vec3 uColor;
void main() {
    gl_FragColor = vec4(uColor, 1.0);
}
)GLSL";
    return link_program(kVertexShader, kFragmentShader, {{0, "aPosition"}}, error);
}

GLuint build_splash_program(std::string* error) {
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
void main() {
    gl_FragColor = texture2D(uTexture, vTexCoord);
}
)GLSL";
    return link_program(
        kVertexShader,
        kFragmentShader,
        {{0, "aPosition"}, {1, "aTexCoord"}},
        error);
}

void clear_splash_texture() {
    if (g_splash_texture != 0) {
        glDeleteTextures(1, &g_splash_texture);
        g_splash_texture = 0;
    }
    g_splash_width = 0;
    g_splash_height = 0;
    std::lock_guard<std::mutex> lock(g_gl_status_mutex);
    g_splash_status = "not loaded";
}

bool upload_splash_texture(int width, int height, const std::uint32_t* argb, size_t count) {
    if (width <= 0 || height <= 0 || argb == nullptr ||
        count != static_cast<size_t>(width) * static_cast<size_t>(height) ||
        count > 16777216u || g_splash_program == 0) {
        return false;
    }

    std::vector<std::uint8_t> rgba(count * 4u);
    for (size_t index = 0; index < count; ++index) {
        const std::uint32_t pixel = argb[index];
        rgba[index * 4u + 0u] = static_cast<std::uint8_t>((pixel >> 16u) & 0xffu);
        rgba[index * 4u + 1u] = static_cast<std::uint8_t>((pixel >> 8u) & 0xffu);
        rgba[index * 4u + 2u] = static_cast<std::uint8_t>(pixel & 0xffu);
        rgba[index * 4u + 3u] = static_cast<std::uint8_t>((pixel >> 24u) & 0xffu);
    }

    clear_splash_texture();
    glGenTextures(1, &g_splash_texture);
    if (g_splash_texture == 0) return false;

    glBindTexture(GL_TEXTURE_2D, g_splash_texture);
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
        glDeleteTextures(1, &g_splash_texture);
        g_splash_texture = 0;
        std::lock_guard<std::mutex> lock(g_gl_status_mutex);
        std::ostringstream message;
        message << "upload failed (GL error 0x" << std::hex << error << ")";
        g_splash_status = message.str();
        return false;
    }

    g_splash_width = width;
    g_splash_height = height;
    {
        std::lock_guard<std::mutex> lock(g_gl_status_mutex);
        std::ostringstream message;
        message << "loaded " << width << "x" << height;
        g_splash_status = message.str();
    }
    return true;
}

void draw_splash() {
    const int surface_width = g_width.load(std::memory_order_relaxed);
    const int surface_height = g_height.load(std::memory_order_relaxed);
    if (g_splash_texture == 0 || g_splash_program == 0 ||
        g_splash_width <= 0 || g_splash_height <= 0 ||
        surface_width <= 0 || surface_height <= 0) {
        return;
    }

    const GLfloat image_aspect = static_cast<GLfloat>(g_splash_width) /
        static_cast<GLfloat>(g_splash_height);
    const GLfloat surface_aspect = static_cast<GLfloat>(surface_width) /
        static_cast<GLfloat>(surface_height);
    GLfloat half_width = 0.90f;
    GLfloat half_height = 0.90f;
    if (image_aspect > surface_aspect) {
        half_height = half_width * surface_aspect / image_aspect;
    } else {
        half_width = half_height * image_aspect / surface_aspect;
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

    glUseProgram(g_splash_program);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, g_splash_texture);
    glUniform1i(g_splash_sampler_uniform, 0);
    glEnableVertexAttribArray(0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, vertices);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 0, kTexCoords);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    glDisableVertexAttribArray(1);
    glDisableVertexAttribArray(0);
    glBindTexture(GL_TEXTURE_2D, 0);
}

void draw_fallback_phase() {
    if (g_program == 0) return;
    static constexpr GLfloat kVertices[] = {
        0.0f, 0.55f,
        -0.48f, -0.45f,
        0.48f, -0.45f,
    };
    const RenderPhase phase = current_render_phase();
    glUseProgram(g_program);
    glUniform3f(g_color_uniform, phase.red, phase.green, phase.blue);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, kVertices);
    glDrawArrays(GL_TRIANGLES, 0, 3);
    glDisableVertexAttribArray(0);
}

void on_surface_created() {
    g_frame_count.store(0, std::memory_order_relaxed);
    g_surface_generation.fetch_add(1, std::memory_order_relaxed);
    nevergone::game_clock::reset();
    g_splash_texture = 0;
    g_splash_width = 0;
    g_splash_height = 0;
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);

    std::string shader_error;
    g_program = build_smoke_program(&shader_error);
    if (g_program != 0) {
        g_color_uniform = glGetUniformLocation(g_program, "uColor");
        if (g_color_uniform < 0) {
            shader_error = "uColor uniform unavailable";
            glDeleteProgram(g_program);
            g_program = 0;
        }
    }

    std::string splash_shader_error;
    g_splash_program = build_splash_program(&splash_shader_error);
    if (g_splash_program != 0) {
        g_splash_sampler_uniform = glGetUniformLocation(g_splash_program, "uTexture");
        if (g_splash_sampler_uniform < 0) {
            splash_shader_error = "uTexture uniform unavailable";
            glDeleteProgram(g_splash_program);
            g_splash_program = 0;
        }
    }

    std::lock_guard<std::mutex> lock(g_gl_status_mutex);
    g_gl_vendor = gl_string(GL_VENDOR);
    g_gl_renderer = gl_string(GL_RENDERER);
    g_gl_version = gl_string(GL_VERSION);
    g_shader_status = g_program != 0 ? "ok" : shader_error;
    g_splash_status = g_splash_program != 0 ? "shader ready; texture not loaded" : splash_shader_error;
}

void on_surface_changed(int width, int height) {
    g_width.store(width, std::memory_order_relaxed);
    g_height.store(height, std::memory_order_relaxed);
    glViewport(0, 0, width, height);
}

void on_draw_frame() {
    (void)nevergone::game_clock::advance();
    glClear(GL_COLOR_BUFFER_BIT);
    if (g_splash_texture != 0) {
        draw_splash();
    } else {
        draw_fallback_phase();
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

void on_app_pause() {
    g_app_resumed.store(false, std::memory_order_relaxed);
    g_pause_count.fetch_add(1, std::memory_order_relaxed);
    nevergone::game_clock::pause();
}

void on_app_resume() {
    nevergone::game_clock::resume();
    g_app_resumed.store(true, std::memory_order_relaxed);
    g_resume_count.fetch_add(1, std::memory_order_relaxed);
}

std::string status_report() {
    std::ostringstream out;
    out << "app lifecycle: "
        << (g_app_resumed.load(std::memory_order_relaxed) ? "resumed" : "paused") << "\n";
    out << "resume events: " << g_resume_count.load(std::memory_order_relaxed) << "\n";
    out << "pause events: " << g_pause_count.load(std::memory_order_relaxed) << "\n";
    out << "GL surface generation: " << g_surface_generation.load(std::memory_order_relaxed) << "\n";
    out << "render surface: " << g_width.load(std::memory_order_relaxed)
        << "x" << g_height.load(std::memory_order_relaxed) << "\n";
    out << "render frames: " << g_frame_count.load(std::memory_order_relaxed) << "\n";
    out << nevergone::game_clock::status_report();
    out << "client render phase: " << current_render_phase().name << "\n";
    {
        std::lock_guard<std::mutex> lock(g_gl_status_mutex);
        out << "GL vendor: " << (g_gl_vendor.empty() ? "pending" : g_gl_vendor) << "\n";
        out << "GL renderer: " << (g_gl_renderer.empty() ? "pending" : g_gl_renderer) << "\n";
        out << "GL version: " << (g_gl_version.empty() ? "pending" : g_gl_version) << "\n";
        out << "shader pipeline: " << g_shader_status << "\n";
        out << "imported splash: " << g_splash_status << "\n";
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

extern "C" JNIEXPORT void JNICALL
Java_org_nevergone_recomp_GameSurfaceView_nativeClearSplashTexture(JNIEnv*, jclass) {
    nevergone::render::clear_splash_texture();
}

extern "C" JNIEXPORT jboolean JNICALL
Java_org_nevergone_recomp_GameSurfaceView_nativeUploadSplashTexture(
    JNIEnv* env,
    jclass,
    jint width,
    jint height,
    jintArray pixels) {
    if (pixels == nullptr || width <= 0 || height <= 0) return JNI_FALSE;
    const jsize length = env->GetArrayLength(pixels);
    const size_t expected = static_cast<size_t>(width) * static_cast<size_t>(height);
    if (static_cast<size_t>(length) != expected || expected > 16777216u) return JNI_FALSE;

    jint* values = env->GetIntArrayElements(pixels, nullptr);
    if (values == nullptr) return JNI_FALSE;
    const bool uploaded = nevergone::render::upload_splash_texture(
        static_cast<int>(width),
        static_cast<int>(height),
        reinterpret_cast<const std::uint32_t*>(values),
        expected);
    env->ReleaseIntArrayElements(pixels, values, JNI_ABORT);
    return uploaded ? JNI_TRUE : JNI_FALSE;
}

extern "C" JNIEXPORT void JNICALL
Java_org_nevergone_recomp_MainActivity_nativeOnAppPause(JNIEnv*, jclass) {
    nevergone::render::on_app_pause();
}

extern "C" JNIEXPORT void JNICALL
Java_org_nevergone_recomp_MainActivity_nativeOnAppResume(JNIEnv*, jclass) {
    nevergone::render::on_app_resume();
}
