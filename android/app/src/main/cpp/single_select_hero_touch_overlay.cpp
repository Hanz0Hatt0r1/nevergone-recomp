#include <GLES2/gl2.h>
#include <jni.h>

#include <atomic>
#include <cstdint>
#include <mutex>
#include <vector>

#include "fresh_role_compat_state.h"
#include "offline_startup_flow.h"
#include "single_select_hero_rune_layout.h"
#include "single_select_hero_touch_state.h"

namespace nevergone::single_select_hero_touch_overlay {
namespace {

constexpr int kRuneCount = single_select_hero_rune_layout::kRuneCount;
constexpr int kActionDown = 0;
constexpr int kActionUp = 1;
constexpr int kActionMove = 2;
constexpr int kActionCancel = 3;
constexpr int kActionOutside = 4;
constexpr int kActionPointerDown = 5;
constexpr int kActionPointerUp = 6;

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
PositionedTexture g_glow[kRuneCount]{};
std::vector<std::uint32_t> g_glow_pixels[kRuneCount];
std::mutex g_hit_mutex;
single_select_hero_rune_layout::FrameGeometry g_hit_geometry[kRuneCount]{};
std::atomic<int> g_surface_width{0};
std::atomic<int> g_surface_height{0};

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

void clear_assets() {
    for (int index = 0; index < kRuneCount; ++index) {
        delete_texture(&g_glow[index]);
        g_glow[index] = {};
        g_glow_pixels[index].clear();
        g_glow_pixels[index].shrink_to_fit();
    }
    {
        std::lock_guard<std::mutex> lock(g_hit_mutex);
        for (auto& geometry : g_hit_geometry) geometry = {};
    }
    single_select_hero_touch_state::reset();
}

bool valid_geometry(
        int width,
        int height,
        int left,
        int top,
        int source_width,
        int source_height) {
    single_select_hero_rune_layout::FrameGeometry geometry;
    geometry.width = width;
    geometry.height = height;
    geometry.left = left;
    geometry.top = top;
    geometry.source_width = source_width;
    geometry.source_height = source_height;
    return single_select_hero_rune_layout::valid_frame(geometry);
}

bool configure_hitbox(
        int tag,
        int width,
        int height,
        int left,
        int top,
        int source_width,
        int source_height) {
    if (!single_select_hero_rune_layout::valid_tag(tag) ||
            !valid_geometry(width, height, left, top, source_width, source_height)) {
        return false;
    }

    single_select_hero_rune_layout::FrameGeometry geometry;
    geometry.width = width;
    geometry.height = height;
    geometry.left = left;
    geometry.top = top;
    geometry.source_width = source_width;
    geometry.source_height = source_height;
    std::lock_guard<std::mutex> lock(g_hit_mutex);
    g_hit_geometry[tag - 1] = geometry;
    return true;
}

bool upload_glow(
        int tag,
        int width,
        int height,
        int left,
        int top,
        int source_width,
        int source_height,
        const std::uint32_t* argb,
        std::size_t count) {
    if (!single_select_hero_rune_layout::valid_tag(tag) || argb == nullptr ||
            !valid_geometry(width, height, left, top, source_width, source_height) ||
            count != static_cast<std::size_t>(width) * static_cast<std::size_t>(height) ||
            count > 16777216u) {
        return false;
    }

    PositionedTexture& asset = g_glow[tag - 1];
    delete_texture(&asset);
    asset = {};
    asset.width = width;
    asset.height = height;
    asset.left = left;
    asset.top = top;
    asset.source_width = source_width;
    asset.source_height = source_height;
    g_glow_pixels[tag - 1].assign(argb, argb + count);
    return true;
}

GLuint create_texture(const std::uint32_t* argb, std::size_t count, int width, int height) {
    if (argb == nullptr || width <= 0 || height <= 0 ||
            count != static_cast<std::size_t>(width) * static_cast<std::size_t>(height)) {
        return 0;
    }
    std::vector<std::uint8_t> rgba(count * 4u);
    for (std::size_t index = 0; index < count; ++index) {
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

bool ensure_texture(int tag) {
    if (!single_select_hero_rune_layout::valid_tag(tag)) return false;
    PositionedTexture& asset = g_glow[tag - 1];
    if (asset.texture != 0) return true;
    const auto& pixels = g_glow_pixels[tag - 1];
    if (pixels.size() !=
            static_cast<std::size_t>(asset.width) * static_cast<std::size_t>(asset.height)) {
        return false;
    }
    asset.texture = create_texture(pixels.data(), pixels.size(), asset.width, asset.height);
    return asset.texture != 0;
}

void on_surface_created() {
    for (auto& asset : g_glow) asset.texture = 0;
    if (g_program != 0) glDeleteProgram(g_program);
    g_program = build_program();
    g_sampler = g_program != 0 ? glGetUniformLocation(g_program, "uTexture") : -1;
}

void on_surface_changed(int width, int height) {
    g_surface_width.store(width);
    g_surface_height.store(height);
}

bool career_surface_active() {
    return offline_startup_flow::snapshot().route ==
            offline_startup_flow::Route::kOpeningDialogue &&
        fresh_role_compat_state::snapshot().mode ==
            fresh_role_compat_state::Mode::kCareerSelection;
}

single_select_hero_rune_layout::FrameGeometry hit_geometry(int tag) {
    if (!single_select_hero_rune_layout::valid_tag(tag)) return {};
    std::lock_guard<std::mutex> lock(g_hit_mutex);
    return g_hit_geometry[tag - 1];
}

bool point_inside_tag(int tag, float x, float y) {
    const int width = g_surface_width.load();
    const int height = g_surface_height.load();
    const auto rect = single_select_hero_rune_layout::content_rect_for_surface(
        hit_geometry(tag), tag, width, height);
    return single_select_hero_rune_layout::contains(rect, x, y);
}

int tag_at(float x, float y) {
    for (int tag = 1; tag <= kRuneCount; ++tag) {
        if (point_inside_tag(tag, x, y)) return tag;
    }
    return 0;
}

bool on_touch(int action, int pointer_id, float x, float y) {
    if (!career_surface_active()) {
        single_select_hero_touch_state::reset();
        return false;
    }

    if (action == kActionDown || action == kActionPointerDown) {
        const int tag = tag_at(x, y);
        return tag != 0 && single_select_hero_touch_state::begin(pointer_id, tag);
    }

    const auto gesture = single_select_hero_touch_state::snapshot();
    if (gesture.pointer_id != pointer_id || gesture.armed_tag == 0) return false;

    if (action == kActionMove) {
        single_select_hero_touch_state::move(
            pointer_id, point_inside_tag(gesture.armed_tag, x, y));
        return true;
    }

    if (action == kActionUp || action == kActionPointerUp) {
        int activated_tag = 0;
        const bool inside = point_inside_tag(gesture.armed_tag, x, y);
        if (!single_select_hero_touch_state::release(
                pointer_id, inside, &activated_tag)) {
            return false;
        }
        if (activated_tag != 0) {
            // Reuse the explicit temporary transition-collapse boundary already
            // owned by the fresh-role compatibility path. The career integer is
            // still dispatched unchanged into the recovered semantic selector.
            fresh_role_compat_state::select_career(activated_tag);
        }
        return true;
    }

    if (action == kActionCancel || action == kActionOutside) {
        single_select_hero_touch_state::cancel(pointer_id);
        return true;
    }
    return true;
}

void draw() {
    if (!career_surface_active() || g_program == 0 || g_sampler < 0) {
        single_select_hero_touch_state::reset();
        return;
    }

    const auto gesture = single_select_hero_touch_state::snapshot();
    if (!gesture.inside || !single_select_hero_rune_layout::valid_tag(gesture.armed_tag)) return;
    const int tag = gesture.armed_tag;
    if (!ensure_texture(tag)) return;

    PositionedTexture& asset = g_glow[tag - 1];
    single_select_hero_rune_layout::FrameGeometry geometry;
    geometry.width = asset.width;
    geometry.height = asset.height;
    geometry.left = asset.left;
    geometry.top = asset.top;
    geometry.source_width = asset.source_width;
    geometry.source_height = asset.source_height;
    const int width = g_surface_width.load();
    const int height = g_surface_height.load();
    const auto quad = single_select_hero_rune_layout::quad_for_surface(
        geometry, tag, width, height);
    if (!quad.valid) return;

    static constexpr GLfloat kTexCoords[] = {
        0.0f, 0.0f,
        0.0f, 1.0f,
        1.0f, 0.0f,
        1.0f, 1.0f,
    };
    const GLfloat vertices[] = {
        quad.x0, quad.y0,
        quad.x0, quad.y1,
        quad.x1, quad.y0,
        quad.x1, quad.y1,
    };

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
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
    glDisable(GL_BLEND);
}

}  // namespace
}  // namespace nevergone::single_select_hero_touch_overlay

extern "C" JNIEXPORT void JNICALL
Java_org_nevergone_recomp_GameSurfaceView_nativeOnSingleSelectHeroTouchSurfaceCreated(
        JNIEnv*, jclass) {
    nevergone::single_select_hero_touch_overlay::on_surface_created();
}

extern "C" JNIEXPORT void JNICALL
Java_org_nevergone_recomp_GameSurfaceView_nativeOnSingleSelectHeroTouchSurfaceChanged(
        JNIEnv*, jclass, jint width, jint height) {
    nevergone::single_select_hero_touch_overlay::on_surface_changed(
        static_cast<int>(width), static_cast<int>(height));
}

extern "C" JNIEXPORT void JNICALL
Java_org_nevergone_recomp_GameSurfaceView_nativeDrawSingleSelectHeroTouchOverlay(
        JNIEnv*, jclass) {
    nevergone::single_select_hero_touch_overlay::draw();
}

extern "C" JNIEXPORT jboolean JNICALL
Java_org_nevergone_recomp_GameSurfaceView_nativeOnSingleSelectHeroTouch(
        JNIEnv*, jclass, jint action, jint pointer_id, jfloat x, jfloat y) {
    return nevergone::single_select_hero_touch_overlay::on_touch(
               static_cast<int>(action),
               static_cast<int>(pointer_id),
               static_cast<float>(x),
               static_cast<float>(y))
        ? JNI_TRUE
        : JNI_FALSE;
}

extern "C" JNIEXPORT void JNICALL
Java_org_nevergone_recomp_SingleSelectHeroBaseComposer_nativeClearCareerRuneTouchAssets(
        JNIEnv*, jclass) {
    nevergone::single_select_hero_touch_overlay::clear_assets();
}

extern "C" JNIEXPORT jboolean JNICALL
Java_org_nevergone_recomp_SingleSelectHeroBaseComposer_nativeConfigureCareerRuneHitbox(
        JNIEnv*,
        jclass,
        jint tag,
        jint width,
        jint height,
        jint left,
        jint top,
        jint source_width,
        jint source_height) {
    return nevergone::single_select_hero_touch_overlay::configure_hitbox(
               static_cast<int>(tag),
               static_cast<int>(width),
               static_cast<int>(height),
               static_cast<int>(left),
               static_cast<int>(top),
               static_cast<int>(source_width),
               static_cast<int>(source_height))
        ? JNI_TRUE
        : JNI_FALSE;
}

extern "C" JNIEXPORT jboolean JNICALL
Java_org_nevergone_recomp_SingleSelectHeroBaseComposer_nativeUploadCareerRuneGlow(
        JNIEnv* env,
        jclass,
        jint tag,
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
    if (static_cast<std::size_t>(length) != expected) return JNI_FALSE;
    jint* values = env->GetIntArrayElements(pixels, nullptr);
    if (values == nullptr) return JNI_FALSE;
    const bool uploaded = nevergone::single_select_hero_touch_overlay::upload_glow(
        static_cast<int>(tag),
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
