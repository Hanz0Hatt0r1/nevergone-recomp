#include "tutorial_dialogue_overlay.h"

#include <GLES2/gl2.h>
#include <jni.h>

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <mutex>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include "login_localization_csv.h"
#include "tutorial_session_state.h"

namespace nevergone::tutorial_dialogue_overlay {
namespace {

constexpr float kDesignWidth = 1136.0f;
constexpr float kDesignHeight = 640.0f;
constexpr float kTextLeft = 80.0f;
constexpr float kTextTop = 150.0f;
constexpr int kActionDown = 0;
constexpr int kActionUp = 1;
constexpr int kActionMove = 2;
constexpr int kActionCancel = 3;
constexpr int kActionPointerDown = 5;
constexpr int kActionPointerUp = 6;
constexpr int kMaxPages = 64;
constexpr std::size_t kMaxPixels = 16u * 1024u * 1024u;

struct Asset {
    int width = 0;
    int height = 0;
    std::vector<std::uint32_t> pixels;
};

struct State {
    Asset background;
    std::vector<Asset> pages;
    int dialogue_id = 0;
    bool auto_play = false;
    bool ready = false;
    int current_page = 0;
    int captured_pointer = -1;
    std::uint64_t asset_generation = 0;
    std::uint64_t draw_attempt_count = 0;
    std::uint64_t drawn_frame_count = 0;
    std::uint64_t accepted_begin_count = 0;
    std::uint64_t accepted_end_count = 0;
    std::uint64_t page_advance_count = 0;
    std::uint64_t completion_count = 0;
    std::uint64_t cancel_count = 0;
    std::uint64_t auto_play_block_count = 0;
};

struct SurfaceMetrics {
    int width = 0;
    int height = 0;
    float scale = 0.0f;
    float offset_x = 0.0f;
    float offset_y = 0.0f;
};

std::mutex g_mutex;
State g_state;
GLuint g_program = 0;
GLint g_sampler = -1;
GLuint g_background_texture = 0;
GLuint g_page_texture = 0;
std::uint64_t g_texture_generation = 0;
int g_texture_page = -1;
std::atomic<int> g_surface_width{0};
std::atomic<int> g_surface_height{0};

bool valid_asset(const Asset& asset) {
    return asset.width > 0 && asset.height > 0 &&
        asset.pixels.size() ==
            static_cast<std::size_t>(asset.width) * static_cast<std::size_t>(asset.height) &&
        asset.pixels.size() <= kMaxPixels;
}

bool make_asset(
        int width,
        int height,
        const std::uint32_t* argb,
        std::size_t count,
        Asset* output) {
    if (output == nullptr || width <= 0 || height <= 0 || argb == nullptr ||
            count == 0 || count > kMaxPixels ||
            count != static_cast<std::size_t>(width) * static_cast<std::size_t>(height)) {
        return false;
    }
    Asset asset;
    asset.width = width;
    asset.height = height;
    asset.pixels.assign(argb, argb + count);
    if (!valid_asset(asset)) return false;
    *output = std::move(asset);
    return true;
}

bool current_surface(SurfaceMetrics* output) {
    if (output == nullptr) return false;
    const int width = g_surface_width.load(std::memory_order_relaxed);
    const int height = g_surface_height.load(std::memory_order_relaxed);
    if (width <= 0 || height <= 0) return false;
    const float scale = std::min(
        static_cast<float>(width) / kDesignWidth,
        static_cast<float>(height) / kDesignHeight);
    if (!std::isfinite(scale) || scale <= 0.0f) return false;
    output->width = width;
    output->height = height;
    output->scale = scale;
    output->offset_x = (static_cast<float>(width) - kDesignWidth * scale) * 0.5f;
    output->offset_y = (static_cast<float>(height) - kDesignHeight * scale) * 0.5f;
    return true;
}

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

void release_textures() {
    if (g_background_texture != 0) glDeleteTextures(1, &g_background_texture);
    if (g_page_texture != 0) glDeleteTextures(1, &g_page_texture);
    g_background_texture = 0;
    g_page_texture = 0;
    g_texture_generation = 0;
    g_texture_page = -1;
}

GLuint create_texture(const Asset& asset) {
    if (!valid_asset(asset)) return 0;
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
    if (texture == 0) return 0;
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
        asset.width,
        asset.height,
        0,
        GL_RGBA,
        GL_UNSIGNED_BYTE,
        rgba.data());
    const GLenum error = glGetError();
    glBindTexture(GL_TEXTURE_2D, 0);
    if (error != GL_NO_ERROR) {
        glDeleteTextures(1, &texture);
        return 0;
    }
    return texture;
}

bool ensure_textures() {
    Asset background;
    Asset page;
    std::uint64_t generation = 0;
    int page_index = -1;
    {
        std::lock_guard<std::mutex> lock(g_mutex);
        if (!g_state.ready || !valid_asset(g_state.background) || g_state.pages.empty() ||
                g_state.current_page < 0 ||
                g_state.current_page >= static_cast<int>(g_state.pages.size()) ||
                !valid_asset(g_state.pages[static_cast<std::size_t>(g_state.current_page)])) {
            return false;
        }
        generation = g_state.asset_generation;
        page_index = g_state.current_page;
        if (g_texture_generation == generation && g_texture_page == page_index &&
                g_background_texture != 0 && g_page_texture != 0) {
            return true;
        }
        background = g_state.background;
        page = g_state.pages[static_cast<std::size_t>(page_index)];
    }

    release_textures();
    const GLuint background_texture = create_texture(background);
    if (background_texture == 0) return false;
    const GLuint page_texture = create_texture(page);
    if (page_texture == 0) {
        glDeleteTextures(1, &background_texture);
        return false;
    }
    g_background_texture = background_texture;
    g_page_texture = page_texture;
    g_texture_generation = generation;
    g_texture_page = page_index;
    return true;
}

void draw_rect(
        GLuint texture,
        const SurfaceMetrics& surface,
        float design_center_x,
        float design_center_y,
        float design_width,
        float design_height) {
    if (texture == 0 || design_width <= 0.0f || design_height <= 0.0f) return;
    const float center_x = surface.offset_x + design_center_x * surface.scale;
    const float center_y = surface.offset_y + (kDesignHeight - design_center_y) * surface.scale;
    const float half_width = design_width * surface.scale * 0.5f;
    const float half_height = design_height * surface.scale * 0.5f;
    const float left = center_x - half_width;
    const float right = center_x + half_width;
    const float top = center_y - half_height;
    const float bottom = center_y + half_height;

    const GLfloat vertices[] = {
        left * 2.0f / static_cast<float>(surface.width) - 1.0f,
        1.0f - top * 2.0f / static_cast<float>(surface.height),
        left * 2.0f / static_cast<float>(surface.width) - 1.0f,
        1.0f - bottom * 2.0f / static_cast<float>(surface.height),
        right * 2.0f / static_cast<float>(surface.width) - 1.0f,
        1.0f - top * 2.0f / static_cast<float>(surface.height),
        right * 2.0f / static_cast<float>(surface.width) - 1.0f,
        1.0f - bottom * 2.0f / static_cast<float>(surface.height),
    };
    static constexpr GLfloat kTexCoords[] = {
        0.0f, 0.0f,
        0.0f, 1.0f,
        1.0f, 0.0f,
        1.0f, 1.0f,
    };

    glBindTexture(GL_TEXTURE_2D, texture);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, vertices);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 0, kTexCoords);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
}

}  // namespace

void on_surface_created() {
    g_background_texture = 0;
    g_page_texture = 0;
    g_texture_generation = 0;
    g_texture_page = -1;
    if (g_program != 0) glDeleteProgram(g_program);
    g_program = build_program();
    g_sampler = g_program != 0 ? glGetUniformLocation(g_program, "uTexture") : -1;
}

void on_surface_changed(int width, int height) {
    g_surface_width.store(width, std::memory_order_relaxed);
    g_surface_height.store(height, std::memory_order_relaxed);
}

void clear_assets() {
    std::lock_guard<std::mutex> lock(g_mutex);
    g_state.background = {};
    g_state.pages.clear();
    g_state.dialogue_id = 0;
    g_state.auto_play = false;
    g_state.ready = false;
    g_state.current_page = 0;
    g_state.captured_pointer = -1;
    ++g_state.asset_generation;
}

bool configure_sequence(int dialogue_id, bool auto_play, int page_count) {
    if (dialogue_id != kStartupGuideDialogueId || page_count <= 0 || page_count > kMaxPages) {
        return false;
    }
    std::lock_guard<std::mutex> lock(g_mutex);
    g_state.background = {};
    g_state.pages.assign(static_cast<std::size_t>(page_count), Asset{});
    g_state.dialogue_id = dialogue_id;
    g_state.auto_play = auto_play;
    g_state.ready = false;
    g_state.current_page = 0;
    g_state.captured_pointer = -1;
    ++g_state.asset_generation;
    return true;
}

bool upload_background(
        int width,
        int height,
        const std::uint32_t* argb,
        std::size_t count) {
    Asset asset;
    if (!make_asset(width, height, argb, count, &asset)) return false;
    std::lock_guard<std::mutex> lock(g_mutex);
    if (g_state.dialogue_id != kStartupGuideDialogueId || g_state.pages.empty()) return false;
    g_state.background = std::move(asset);
    ++g_state.asset_generation;
    return true;
}

bool upload_page(
        int page_index,
        int width,
        int height,
        const std::uint32_t* argb,
        std::size_t count) {
    Asset asset;
    if (!make_asset(width, height, argb, count, &asset)) return false;
    std::lock_guard<std::mutex> lock(g_mutex);
    if (page_index < 0 || page_index >= static_cast<int>(g_state.pages.size())) return false;
    g_state.pages[static_cast<std::size_t>(page_index)] = std::move(asset);
    ++g_state.asset_generation;
    return true;
}

bool commit_assets() {
    std::lock_guard<std::mutex> lock(g_mutex);
    if (g_state.dialogue_id != kStartupGuideDialogueId || !valid_asset(g_state.background) ||
            g_state.pages.empty()) {
        g_state.ready = false;
        return false;
    }
    for (const Asset& page : g_state.pages) {
        if (!valid_asset(page)) {
            g_state.ready = false;
            return false;
        }
    }
    g_state.ready = true;
    g_state.current_page = 0;
    g_state.captured_pointer = -1;
    ++g_state.asset_generation;
    return true;
}

bool draw() {
    if (tutorial_session_state::snapshot().phase != tutorial_session_state::Phase::kDialogue) {
        return false;
    }
    {
        std::lock_guard<std::mutex> lock(g_mutex);
        ++g_state.draw_attempt_count;
    }

    SurfaceMetrics surface;
    if (g_program == 0 || g_sampler < 0 || !current_surface(&surface) || !ensure_textures()) {
        return false;
    }

    Asset background;
    Asset page;
    {
        std::lock_guard<std::mutex> lock(g_mutex);
        background = g_state.background;
        page = g_state.pages[static_cast<std::size_t>(g_state.current_page)];
    }

    const float background_width = kDesignWidth;
    const float background_height =
        static_cast<float>(background.height) * kDesignWidth /
        static_cast<float>(background.width);
    const float page_center_x = kTextLeft + static_cast<float>(page.width) * 0.5f;
    const float page_center_y = kTextTop - static_cast<float>(page.height) * 0.5f;

    while (glGetError() != GL_NO_ERROR) {
    }
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glUseProgram(g_program);
    glActiveTexture(GL_TEXTURE0);
    glUniform1i(g_sampler, 0);
    glEnableVertexAttribArray(0);
    glEnableVertexAttribArray(1);
    draw_rect(
        g_background_texture,
        surface,
        kDesignWidth * 0.5f,
        kDesignHeight * 0.5f,
        background_width,
        background_height);
    draw_rect(
        g_page_texture,
        surface,
        page_center_x,
        page_center_y,
        static_cast<float>(page.width),
        static_cast<float>(page.height));
    glDisableVertexAttribArray(1);
    glDisableVertexAttribArray(0);
    glBindTexture(GL_TEXTURE_2D, 0);
    glDisable(GL_BLEND);

    if (glGetError() != GL_NO_ERROR) return false;
    std::lock_guard<std::mutex> lock(g_mutex);
    ++g_state.drawn_frame_count;
    return true;
}

bool on_touch(int action, int pointer_id, float surface_x, float surface_y) {
    (void)surface_x;
    (void)surface_y;
    if (tutorial_session_state::snapshot().phase != tutorial_session_state::Phase::kDialogue) {
        return false;
    }

    if (action == kActionCancel) {
        std::lock_guard<std::mutex> lock(g_mutex);
        if (g_state.captured_pointer >= 0) ++g_state.cancel_count;
        g_state.captured_pointer = -1;
        return true;
    }

    if (action == kActionDown || action == kActionPointerDown) {
        std::lock_guard<std::mutex> lock(g_mutex);
        if (g_state.ready && g_state.captured_pointer < 0) {
            g_state.captured_pointer = pointer_id;
            ++g_state.accepted_begin_count;
        }
        return true;
    }

    if (action == kActionMove) return true;

    if (action == kActionUp || action == kActionPointerUp) {
        bool captured = false;
        bool auto_play = false;
        bool complete = false;
        {
            std::lock_guard<std::mutex> lock(g_mutex);
            captured = g_state.captured_pointer == pointer_id;
            if (captured) {
                g_state.captured_pointer = -1;
                ++g_state.accepted_end_count;
            }
            if (!captured || !g_state.ready) return true;
            auto_play = g_state.auto_play;
            if (auto_play) {
                ++g_state.auto_play_block_count;
                return true;
            }
            if (g_state.current_page + 1 < static_cast<int>(g_state.pages.size())) {
                ++g_state.current_page;
                ++g_state.page_advance_count;
            } else {
                complete = true;
            }
        }
        if (complete && tutorial_session_state::on_dialogue_complete()) {
            std::lock_guard<std::mutex> lock(g_mutex);
            ++g_state.completion_count;
        }
        return true;
    }

    return true;
}

std::string status_report() {
    std::lock_guard<std::mutex> lock(g_mutex);
    std::ostringstream out;
    out << "Tutorial DialogueUI\n";
    out << "dialogue id: " << g_state.dialogue_id << "\n";
    out << "assets ready: " << (g_state.ready ? "yes" : "no") << "\n";
    out << "auto play: " << (g_state.auto_play ? "yes" : "no") << "\n";
    out << "pages: " << g_state.pages.size() << "\n";
    out << "current page: " << g_state.current_page << "\n";
    out << "asset generation: " << g_state.asset_generation << "\n";
    if (valid_asset(g_state.background)) {
        out << "background pixels: " << g_state.background.width << "x"
            << g_state.background.height << "\n";
    }
    out << "draw attempts: " << g_state.draw_attempt_count << "\n";
    out << "drawn frames: " << g_state.drawn_frame_count << "\n";
    out << "touch begins: " << g_state.accepted_begin_count << "\n";
    out << "touch ends: " << g_state.accepted_end_count << "\n";
    out << "page advances: " << g_state.page_advance_count << "\n";
    out << "dialogue completions: " << g_state.completion_count << "\n";
    out << "cancelled captures: " << g_state.cancel_count << "\n";
    out << "auto-play touch blocks: " << g_state.auto_play_block_count << "\n";
    return out.str();
}

}  // namespace nevergone::tutorial_dialogue_overlay

namespace {

std::string tutorial_dialogue_jstring_to_utf8(JNIEnv* env, jstring value) {
    if (env == nullptr || value == nullptr) return {};
    const char* chars = env->GetStringUTFChars(value, nullptr);
    if (chars == nullptr) return {};
    std::string result(chars);
    env->ReleaseStringUTFChars(value, chars);
    return result;
}

bool tutorial_dialogue_upload_java_asset(
        JNIEnv* env,
        jint width,
        jint height,
        jintArray pixels,
        int page_index) {
    if (env == nullptr || pixels == nullptr || width <= 0 || height <= 0) return false;
    const jsize length = env->GetArrayLength(pixels);
    const std::size_t expected =
        static_cast<std::size_t>(width) * static_cast<std::size_t>(height);
    if (static_cast<std::size_t>(length) != expected || expected > 16u * 1024u * 1024u) {
        return false;
    }
    jint* values = env->GetIntArrayElements(pixels, nullptr);
    if (values == nullptr) return false;
    const bool ok = page_index < 0
        ? nevergone::tutorial_dialogue_overlay::upload_background(
            static_cast<int>(width),
            static_cast<int>(height),
            reinterpret_cast<const std::uint32_t*>(values),
            expected)
        : nevergone::tutorial_dialogue_overlay::upload_page(
            page_index,
            static_cast<int>(width),
            static_cast<int>(height),
            reinterpret_cast<const std::uint32_t*>(values),
            expected);
    env->ReleaseIntArrayElements(pixels, values, JNI_ABORT);
    return ok;
}

}  // namespace

extern "C" JNIEXPORT void JNICALL
Java_org_nevergone_recomp_TutorialDialogueAssetLoader_nativeClear(JNIEnv*, jclass) {
    nevergone::tutorial_dialogue_overlay::clear_assets();
}

extern "C" JNIEXPORT jboolean JNICALL
Java_org_nevergone_recomp_TutorialDialogueAssetLoader_nativeConfigure(
        JNIEnv*,
        jclass,
        jint dialogue_id,
        jboolean auto_play,
        jint page_count) {
    return nevergone::tutorial_dialogue_overlay::configure_sequence(
        static_cast<int>(dialogue_id),
        auto_play == JNI_TRUE,
        static_cast<int>(page_count)) ? JNI_TRUE : JNI_FALSE;
}

extern "C" JNIEXPORT jboolean JNICALL
Java_org_nevergone_recomp_TutorialDialogueAssetLoader_nativeUploadBackground(
        JNIEnv* env,
        jclass,
        jint width,
        jint height,
        jintArray pixels) {
    return tutorial_dialogue_upload_java_asset(env, width, height, pixels, -1)
        ? JNI_TRUE
        : JNI_FALSE;
}

extern "C" JNIEXPORT jboolean JNICALL
Java_org_nevergone_recomp_TutorialDialogueAssetLoader_nativeUploadPage(
        JNIEnv* env,
        jclass,
        jint page_index,
        jint width,
        jint height,
        jintArray pixels) {
    return tutorial_dialogue_upload_java_asset(env, width, height, pixels, page_index)
        ? JNI_TRUE
        : JNI_FALSE;
}

extern "C" JNIEXPORT jboolean JNICALL
Java_org_nevergone_recomp_TutorialDialogueAssetLoader_nativeCommit(JNIEnv*, jclass) {
    return nevergone::tutorial_dialogue_overlay::commit_assets() ? JNI_TRUE : JNI_FALSE;
}

extern "C" JNIEXPORT jstring JNICALL
Java_org_nevergone_recomp_TutorialDialogueAssetLoader_nativeLocalizedString(
        JNIEnv* env,
        jclass,
        jstring files_dir,
        jstring key,
        jint language_column) {
    if (env == nullptr || files_dir == nullptr || key == nullptr || language_column < 0) {
        return nullptr;
    }
    std::string value;
    if (!nevergone::login_localization_csv::resolve_key_file(
            tutorial_dialogue_jstring_to_utf8(env, files_dir),
            tutorial_dialogue_jstring_to_utf8(env, key),
            static_cast<std::size_t>(language_column),
            &value) || value.empty()) {
        return nullptr;
    }
    return env->NewStringUTF(value.c_str());
}
