#include "tutorial_dialogue_runtime.h"

#include <GLES2/gl2.h>
#include <jni.h>

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <mutex>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include "character_name_state.h"
#include "game_clock.h"
#include "login_localization_csv.h"
#include "tutorial_session_state.h"

namespace nevergone::tutorial_dialogue_runtime {
namespace {

constexpr float kDesignWidth = 1136.0f;
constexpr float kDesignHeight = 640.0f;
constexpr std::size_t kMaxPixels = 16u * 1024u * 1024u;
constexpr int kFirstDialogueId = 8;

struct BitmapAsset {
    int width = 0;
    int height = 0;
    std::vector<std::uint32_t> pixels;
};

struct LineAsset {
    int npc = 0;
    int npc_position = 0;
    double appear_seconds = 0.0;
    double show_delay_seconds = 0.0;
    bool fade_in = false;
    bool fade_out = false;
    BitmapAsset portrait;
    BitmapAsset name;
    BitmapAsset text;
    bool uploaded = false;
};

struct RuntimeState {
    Stage stage = Stage::kInactive;
    std::uint64_t generation = 0;
    int dialogue_id = 0;
    int line_index = 0;
    std::uint64_t line_started_tick = 0;
    std::uint64_t first_dialogue_completion_count = 0;
    std::uint64_t draw_attempt_count = 0;
    std::uint64_t drawn_frame_count = 0;

    bool assets_ready = false;
    bool auto_play = false;
    double fade_in_seconds = 0.0;
    double fade_out_seconds = 0.0;
    BitmapAsset background;
    std::vector<LineAsset> lines;
    std::uint64_t asset_revision = 0;
};

struct SurfaceMetrics {
    int width = 0;
    int height = 0;
    float scale = 0.0f;
    float offset_x = 0.0f;
    float offset_y = 0.0f;
};

std::mutex g_mutex;
RuntimeState g_state;
std::atomic<int> g_surface_width{0};
std::atomic<int> g_surface_height{0};

GLuint g_program = 0;
GLint g_sampler = -1;
GLint g_alpha = -1;
GLuint g_background_texture = 0;
GLuint g_portrait_texture = 0;
GLuint g_name_texture = 0;
GLuint g_text_texture = 0;
std::uint64_t g_texture_revision = 0;
int g_texture_line_index = -1;

bool finite_nonnegative(double value) {
    return std::isfinite(value) && value >= 0.0 && value <= 120.0;
}

bool valid_bitmap(const BitmapAsset& asset) {
    if (asset.width <= 0 || asset.height <= 0) return false;
    const std::size_t expected =
        static_cast<std::size_t>(asset.width) * static_cast<std::size_t>(asset.height);
    return expected > 0 && expected <= kMaxPixels && asset.pixels.size() == expected;
}

bool make_bitmap(
        int width,
        int height,
        const std::uint32_t* argb,
        std::size_t count,
        BitmapAsset* output) {
    if (output == nullptr || width <= 0 || height <= 0 || argb == nullptr) return false;
    const std::size_t expected =
        static_cast<std::size_t>(width) * static_cast<std::size_t>(height);
    if (expected == 0 || expected > kMaxPixels || count != expected) return false;
    BitmapAsset asset;
    asset.width = width;
    asset.height = height;
    asset.pixels.assign(argb, argb + count);
    if (!valid_bitmap(asset)) return false;
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
uniform float uAlpha;
void main() {
    vec4 color = texture2D(uTexture, vTexCoord);
    gl_FragColor = vec4(color.rgb, color.a * uAlpha);
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

void delete_texture(GLuint* texture) {
    if (texture == nullptr || *texture == 0) return;
    glDeleteTextures(1, texture);
    *texture = 0;
}

void release_textures() {
    delete_texture(&g_background_texture);
    delete_texture(&g_portrait_texture);
    delete_texture(&g_name_texture);
    delete_texture(&g_text_texture);
    g_texture_revision = 0;
    g_texture_line_index = -1;
}

GLuint create_texture(const BitmapAsset& asset) {
    if (!valid_bitmap(asset)) return 0;
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
    BitmapAsset background;
    BitmapAsset portrait;
    BitmapAsset name;
    BitmapAsset text;
    std::uint64_t revision = 0;
    int line_index = 0;
    {
        std::lock_guard<std::mutex> lock(g_mutex);
        if (!g_state.assets_ready || g_state.stage != Stage::kFirstDialogue ||
                g_state.line_index < 0 ||
                g_state.line_index >= static_cast<int>(g_state.lines.size())) {
            return false;
        }
        revision = g_state.asset_revision;
        line_index = g_state.line_index;
        if (g_texture_revision == revision && g_texture_line_index == line_index &&
                g_background_texture != 0 && g_portrait_texture != 0 &&
                g_name_texture != 0 && g_text_texture != 0) {
            return true;
        }
        const LineAsset& line = g_state.lines[static_cast<std::size_t>(line_index)];
        if (!valid_bitmap(g_state.background) || !line.uploaded ||
                !valid_bitmap(line.portrait) || !valid_bitmap(line.name) ||
                !valid_bitmap(line.text)) {
            return false;
        }
        background = g_state.background;
        portrait = line.portrait;
        name = line.name;
        text = line.text;
    }

    release_textures();
    g_background_texture = create_texture(background);
    if (g_background_texture == 0) return false;
    g_portrait_texture = create_texture(portrait);
    if (g_portrait_texture == 0) {
        release_textures();
        return false;
    }
    g_name_texture = create_texture(name);
    if (g_name_texture == 0) {
        release_textures();
        return false;
    }
    g_text_texture = create_texture(text);
    if (g_text_texture == 0) {
        release_textures();
        return false;
    }
    g_texture_revision = revision;
    g_texture_line_index = line_index;
    return true;
}

void draw_rect(
        GLuint texture,
        const SurfaceMetrics& surface,
        float left,
        float bottom,
        float width,
        float height,
        float alpha) {
    if (texture == 0 || width <= 0.0f || height <= 0.0f || alpha <= 0.0f) return;
    const float right = left + width;
    const float top = bottom + height;

    const float surface_left = surface.offset_x + left * surface.scale;
    const float surface_right = surface.offset_x + right * surface.scale;
    const float surface_top = surface.offset_y + (kDesignHeight - top) * surface.scale;
    const float surface_bottom = surface.offset_y + (kDesignHeight - bottom) * surface.scale;

    const GLfloat vertices[] = {
        surface_left * 2.0f / static_cast<float>(surface.width) - 1.0f,
        1.0f - surface_top * 2.0f / static_cast<float>(surface.height),
        surface_left * 2.0f / static_cast<float>(surface.width) - 1.0f,
        1.0f - surface_bottom * 2.0f / static_cast<float>(surface.height),
        surface_right * 2.0f / static_cast<float>(surface.width) - 1.0f,
        1.0f - surface_top * 2.0f / static_cast<float>(surface.height),
        surface_right * 2.0f / static_cast<float>(surface.width) - 1.0f,
        1.0f - surface_bottom * 2.0f / static_cast<float>(surface.height),
    };
    static constexpr GLfloat kTexCoords[] = {
        0.0f, 0.0f,
        0.0f, 1.0f,
        1.0f, 0.0f,
        1.0f, 1.0f,
    };

    glUniform1f(g_alpha, std::clamp(alpha, 0.0f, 1.0f));
    glBindTexture(GL_TEXTURE_2D, texture);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, vertices);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 0, kTexCoords);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
}

std::uint64_t duration_ticks(const LineAsset& line) {
    const double seconds = std::max(0.0, line.appear_seconds) +
        std::max(0.0, line.show_delay_seconds);
    if (seconds <= 0.0) return 1;
    return std::max<std::uint64_t>(
        1,
        static_cast<std::uint64_t>(std::ceil(seconds / game_clock::kFixedStepSeconds)));
}

float current_line_alpha(
        const LineAsset& line,
        double fade_in_seconds,
        double fade_out_seconds,
        std::uint64_t elapsed_ticks) {
    const double elapsed = static_cast<double>(elapsed_ticks) * game_clock::kFixedStepSeconds;
    const double total = line.appear_seconds + line.show_delay_seconds;
    double alpha = 1.0;
    if (line.fade_in && fade_in_seconds > 0.0 && elapsed < fade_in_seconds) {
        alpha = std::clamp(elapsed / fade_in_seconds, 0.0, 1.0);
    }
    if (line.fade_out && fade_out_seconds > 0.0 && total > 0.0 &&
            elapsed > total - fade_out_seconds) {
        alpha = std::min(
            alpha,
            std::clamp((total - elapsed) / fade_out_seconds, 0.0, 1.0));
    }
    return static_cast<float>(std::clamp(alpha, 0.0, 1.0));
}

}  // namespace

void reset() {
    std::lock_guard<std::mutex> lock(g_mutex);
    g_state = RuntimeState{};
}

bool begin_first_dialogue() {
    if (tutorial_session_state::snapshot().phase != tutorial_session_state::Phase::kDialogue) {
        return false;
    }
    std::lock_guard<std::mutex> lock(g_mutex);
    ++g_state.generation;
    g_state.stage = Stage::kFirstDialogue;
    g_state.dialogue_id = kFirstDialogueId;
    g_state.line_index = 0;
    g_state.line_started_tick = 0;
    g_state.assets_ready = false;
    g_state.auto_play = false;
    g_state.fade_in_seconds = 0.0;
    g_state.fade_out_seconds = 0.0;
    g_state.background = {};
    g_state.lines.clear();
    ++g_state.asset_revision;
    return true;
}

void tick() {
    const auto session = tutorial_session_state::snapshot();
    if (session.phase != tutorial_session_state::Phase::kDialogue) return;

    std::lock_guard<std::mutex> lock(g_mutex);
    if (g_state.stage != Stage::kFirstDialogue || !g_state.assets_ready ||
            !g_state.auto_play || g_state.line_index < 0 ||
            g_state.line_index >= static_cast<int>(g_state.lines.size())) {
        return;
    }
    const std::uint64_t now = game_clock::tick_count();
    if (g_state.line_started_tick == 0) {
        g_state.line_started_tick = now;
        return;
    }
    const std::uint64_t elapsed = now >= g_state.line_started_tick
        ? now - g_state.line_started_tick : 0;
    const LineAsset& line = g_state.lines[static_cast<std::size_t>(g_state.line_index)];
    if (elapsed < duration_ticks(line)) return;

    ++g_state.line_index;
    g_state.line_started_tick = now;
    if (g_state.line_index >= static_cast<int>(g_state.lines.size())) {
        g_state.stage = Stage::kLessonInterlude;
        ++g_state.first_dialogue_completion_count;
        ++g_state.asset_revision;
    }
}

void on_surface_created() {
    release_textures();
    if (g_program != 0) glDeleteProgram(g_program);
    g_program = build_program();
    g_sampler = g_program != 0 ? glGetUniformLocation(g_program, "uTexture") : -1;
    g_alpha = g_program != 0 ? glGetUniformLocation(g_program, "uAlpha") : -1;
}

void on_surface_changed(int width, int height) {
    g_surface_width.store(width, std::memory_order_relaxed);
    g_surface_height.store(height, std::memory_order_relaxed);
}

bool draw() {
    const auto session = tutorial_session_state::snapshot();
    if (session.phase != tutorial_session_state::Phase::kDialogue) return false;

    SurfaceMetrics surface;
    int npc_position = 0;
    int portrait_width = 0;
    int portrait_height = 0;
    int name_width = 0;
    int name_height = 0;
    int text_width = 0;
    int text_height = 0;
    int background_height = 0;
    double fade_in_seconds = 0.0;
    double fade_out_seconds = 0.0;
    LineAsset timing;
    std::uint64_t elapsed_ticks = 0;
    {
        std::lock_guard<std::mutex> lock(g_mutex);
        if (g_state.stage != Stage::kFirstDialogue || !g_state.assets_ready ||
                g_state.line_index < 0 ||
                g_state.line_index >= static_cast<int>(g_state.lines.size())) {
            return false;
        }
        ++g_state.draw_attempt_count;
        const LineAsset& line = g_state.lines[static_cast<std::size_t>(g_state.line_index)];
        npc_position = line.npc_position;
        portrait_width = line.portrait.width;
        portrait_height = line.portrait.height;
        name_width = line.name.width;
        name_height = line.name.height;
        text_width = line.text.width;
        text_height = line.text.height;
        background_height = g_state.background.height;
        fade_in_seconds = g_state.fade_in_seconds;
        fade_out_seconds = g_state.fade_out_seconds;
        timing.appear_seconds = line.appear_seconds;
        timing.show_delay_seconds = line.show_delay_seconds;
        timing.fade_in = line.fade_in;
        timing.fade_out = line.fade_out;
        const std::uint64_t now = game_clock::tick_count();
        elapsed_ticks = now >= g_state.line_started_tick
            ? now - g_state.line_started_tick : 0;
    }

    if (g_program == 0 || g_sampler < 0 || g_alpha < 0 ||
            !current_surface(&surface) || !ensure_textures()) {
        return false;
    }

    const float alpha = current_line_alpha(
        timing, fade_in_seconds, fade_out_seconds, elapsed_ticks);

    while (glGetError() != GL_NO_ERROR) {
    }
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glUseProgram(g_program);
    glActiveTexture(GL_TEXTURE0);
    glUniform1i(g_sampler, 0);
    glEnableVertexAttribArray(0);
    glEnableVertexAttribArray(1);

    // GameConversation centers dialogue_bg.png and scales it horizontally to
    // the visible width. Keep its imported pixel height in design space.
    const float bg_bottom = (kDesignHeight - static_cast<float>(background_height)) * 0.5f;
    draw_rect(
        g_background_texture,
        surface,
        0.0f,
        bg_bottom,
        kDesignWidth,
        static_cast<float>(background_height),
        1.0f);

    // Recovered npcPos switch: 0 anchors the portrait at the right edge,
    // 1 anchors it at the left edge. Tutorial dialogue 8 uses only 0 and 1.
    const bool left_portrait = npc_position == 1;
    const float portrait_left = left_portrait
        ? 0.0f : kDesignWidth - static_cast<float>(portrait_width);
    draw_rect(
        g_portrait_texture,
        surface,
        portrait_left,
        0.0f,
        static_cast<float>(portrait_width),
        static_cast<float>(portrait_height),
        alpha);

    // nextText() places left-side labels at (80,180)/(80,150). For the
    // mirrored player side use the symmetric right inset while retaining the
    // imported 450px content-label width.
    const float name_left = left_portrait
        ? 80.0f : std::max(0.0f, kDesignWidth - 80.0f - static_cast<float>(name_width));
    const float text_left = left_portrait
        ? 80.0f : std::max(0.0f, kDesignWidth - 80.0f - static_cast<float>(text_width));
    draw_rect(
        g_name_texture,
        surface,
        name_left,
        180.0f - static_cast<float>(name_height) * 0.5f,
        static_cast<float>(name_width),
        static_cast<float>(name_height),
        alpha);
    draw_rect(
        g_text_texture,
        surface,
        text_left,
        150.0f - static_cast<float>(text_height) * 0.5f,
        static_cast<float>(text_width),
        static_cast<float>(text_height),
        alpha);

    glDisableVertexAttribArray(1);
    glDisableVertexAttribArray(0);
    glBindTexture(GL_TEXTURE_2D, 0);
    glDisable(GL_BLEND);

    if (glGetError() != GL_NO_ERROR) return false;
    std::lock_guard<std::mutex> lock(g_mutex);
    ++g_state.drawn_frame_count;
    return true;
}

void clear_assets() {
    std::lock_guard<std::mutex> lock(g_mutex);
    g_state.assets_ready = false;
    g_state.auto_play = false;
    g_state.fade_in_seconds = 0.0;
    g_state.fade_out_seconds = 0.0;
    g_state.background = {};
    g_state.lines.clear();
    ++g_state.asset_revision;
}

bool begin_assets(
        int dialogue_id,
        bool auto_play,
        double fade_in_seconds,
        double fade_out_seconds,
        int line_count) {
    if (dialogue_id != kFirstDialogueId || line_count <= 0 || line_count > 32 ||
            !finite_nonnegative(fade_in_seconds) || !finite_nonnegative(fade_out_seconds)) {
        return false;
    }
    std::lock_guard<std::mutex> lock(g_mutex);
    if (g_state.stage != Stage::kFirstDialogue || g_state.dialogue_id != dialogue_id) {
        return false;
    }
    g_state.assets_ready = false;
    g_state.auto_play = auto_play;
    g_state.fade_in_seconds = fade_in_seconds;
    g_state.fade_out_seconds = fade_out_seconds;
    g_state.background = {};
    g_state.lines.assign(static_cast<std::size_t>(line_count), LineAsset{});
    return true;
}

bool upload_background(
        int width,
        int height,
        const std::uint32_t* argb,
        std::size_t count) {
    BitmapAsset asset;
    if (!make_bitmap(width, height, argb, count, &asset)) return false;
    std::lock_guard<std::mutex> lock(g_mutex);
    if (g_state.stage != Stage::kFirstDialogue || g_state.lines.empty()) return false;
    g_state.background = std::move(asset);
    return true;
}

bool upload_line(
        int line_index,
        int npc,
        int npc_position,
        double appear_seconds,
        double show_delay_seconds,
        bool fade_in,
        bool fade_out,
        int portrait_width,
        int portrait_height,
        const std::uint32_t* portrait_argb,
        std::size_t portrait_count,
        int name_width,
        int name_height,
        const std::uint32_t* name_argb,
        std::size_t name_count,
        int text_width,
        int text_height,
        const std::uint32_t* text_argb,
        std::size_t text_count) {
    if (line_index < 0 || npc <= 0 || (npc_position != 0 && npc_position != 1) ||
            !finite_nonnegative(appear_seconds) || !finite_nonnegative(show_delay_seconds)) {
        return false;
    }
    LineAsset line;
    line.npc = npc;
    line.npc_position = npc_position;
    line.appear_seconds = appear_seconds;
    line.show_delay_seconds = show_delay_seconds;
    line.fade_in = fade_in;
    line.fade_out = fade_out;
    if (!make_bitmap(
            portrait_width, portrait_height, portrait_argb, portrait_count, &line.portrait) ||
            !make_bitmap(name_width, name_height, name_argb, name_count, &line.name) ||
            !make_bitmap(text_width, text_height, text_argb, text_count, &line.text)) {
        return false;
    }
    line.uploaded = true;

    std::lock_guard<std::mutex> lock(g_mutex);
    if (g_state.stage != Stage::kFirstDialogue ||
            line_index >= static_cast<int>(g_state.lines.size())) {
        return false;
    }
    g_state.lines[static_cast<std::size_t>(line_index)] = std::move(line);
    return true;
}

bool finish_assets() {
    std::lock_guard<std::mutex> lock(g_mutex);
    if (g_state.stage != Stage::kFirstDialogue || !g_state.auto_play ||
            !valid_bitmap(g_state.background) || g_state.lines.empty()) {
        return false;
    }
    for (const LineAsset& line : g_state.lines) {
        if (!line.uploaded || !valid_bitmap(line.portrait) ||
                !valid_bitmap(line.name) || !valid_bitmap(line.text)) {
            return false;
        }
    }
    g_state.assets_ready = true;
    g_state.line_index = 0;
    g_state.line_started_tick = game_clock::tick_count();
    ++g_state.asset_revision;
    return true;
}

Snapshot snapshot() {
    std::lock_guard<std::mutex> lock(g_mutex);
    Snapshot result;
    result.stage = g_state.stage;
    result.generation = g_state.generation;
    result.dialogue_id = g_state.dialogue_id;
    result.line_index = g_state.line_index;
    result.line_count = static_cast<int>(g_state.lines.size());
    result.assets_ready = g_state.assets_ready;
    result.auto_play = g_state.auto_play;
    result.fade_in_seconds = g_state.fade_in_seconds;
    result.fade_out_seconds = g_state.fade_out_seconds;
    result.line_started_tick = g_state.line_started_tick;
    result.first_dialogue_completion_count = g_state.first_dialogue_completion_count;
    result.draw_attempt_count = g_state.draw_attempt_count;
    result.drawn_frame_count = g_state.drawn_frame_count;
    return result;
}

const char* stage_name(Stage stage) {
    switch (stage) {
        case Stage::kInactive: return "inactive";
        case Stage::kFirstDialogue: return "first-dialogue";
        case Stage::kLessonInterlude: return "lesson-interlude";
        case Stage::kSecondDialogue: return "second-dialogue";
        case Stage::kComplete: return "complete";
    }
    return "unknown";
}

std::string status_report() {
    const Snapshot state = snapshot();
    std::ostringstream out;
    out << "Tutorial dialogue runtime\n";
    out << "stage: " << stage_name(state.stage) << "\n";
    out << "generation: " << state.generation << "\n";
    out << "dialogue id: " << state.dialogue_id << "\n";
    out << "assets ready: " << (state.assets_ready ? "yes" : "no") << "\n";
    out << "auto play: " << (state.auto_play ? "yes" : "no") << "\n";
    out << "line: " << state.line_index << "/" << state.line_count << "\n";
    out << "line started tick: " << state.line_started_tick << "\n";
    out << "fade seconds: " << state.fade_in_seconds << "/" << state.fade_out_seconds << "\n";
    out << "first dialogue completions: " << state.first_dialogue_completion_count << "\n";
    out << "draw attempts: " << state.draw_attempt_count << "\n";
    out << "drawn frames: " << state.drawn_frame_count << "\n";
    return out.str();
}

}  // namespace nevergone::tutorial_dialogue_runtime

namespace {

bool copy_pixels(
        JNIEnv* env,
        jintArray pixels,
        int width,
        int height,
        std::vector<std::uint32_t>* output) {
    if (env == nullptr || pixels == nullptr || output == nullptr || width <= 0 || height <= 0) {
        return false;
    }
    const std::size_t expected =
        static_cast<std::size_t>(width) * static_cast<std::size_t>(height);
    if (expected == 0 || expected > kMaxPixels ||
            static_cast<std::size_t>(env->GetArrayLength(pixels)) != expected) {
        return false;
    }
    jint* values = env->GetIntArrayElements(pixels, nullptr);
    if (values == nullptr) return false;
    output->assign(
        reinterpret_cast<std::uint32_t*>(values),
        reinterpret_cast<std::uint32_t*>(values) + expected);
    env->ReleaseIntArrayElements(pixels, values, JNI_ABORT);
    return true;
}

std::string jstring_to_utf8(JNIEnv* env, jstring value) {
    if (env == nullptr || value == nullptr) return {};
    const char* chars = env->GetStringUTFChars(value, nullptr);
    if (chars == nullptr) return {};
    std::string result(chars);
    env->ReleaseStringUTFChars(value, chars);
    return result;
}

}  // namespace

extern "C" JNIEXPORT void JNICALL
Java_org_nevergone_recomp_TutorialDialogueAssetLoader_nativeClear(JNIEnv*, jclass) {
    nevergone::tutorial_dialogue_runtime::clear_assets();
}

extern "C" JNIEXPORT jint JNICALL
Java_org_nevergone_recomp_TutorialDialogueAssetLoader_nativeRequestedDialogueId(JNIEnv*, jclass) {
    return static_cast<jint>(nevergone::tutorial_dialogue_runtime::snapshot().dialogue_id);
}

extern "C" JNIEXPORT jboolean JNICALL
Java_org_nevergone_recomp_TutorialDialogueAssetLoader_nativeBeginAssets(
        JNIEnv*,
        jclass,
        jint dialogue_id,
        jboolean auto_play,
        jdouble fade_in_seconds,
        jdouble fade_out_seconds,
        jint line_count) {
    return nevergone::tutorial_dialogue_runtime::begin_assets(
        static_cast<int>(dialogue_id),
        auto_play == JNI_TRUE,
        static_cast<double>(fade_in_seconds),
        static_cast<double>(fade_out_seconds),
        static_cast<int>(line_count)) ? JNI_TRUE : JNI_FALSE;
}

extern "C" JNIEXPORT jboolean JNICALL
Java_org_nevergone_recomp_TutorialDialogueAssetLoader_nativeUploadBackground(
        JNIEnv* env,
        jclass,
        jint width,
        jint height,
        jintArray pixels) {
    std::vector<std::uint32_t> copied;
    if (!copy_pixels(env, pixels, width, height, &copied)) return JNI_FALSE;
    return nevergone::tutorial_dialogue_runtime::upload_background(
        static_cast<int>(width),
        static_cast<int>(height),
        copied.data(),
        copied.size()) ? JNI_TRUE : JNI_FALSE;
}

extern "C" JNIEXPORT jboolean JNICALL
Java_org_nevergone_recomp_TutorialDialogueAssetLoader_nativeUploadLine(
        JNIEnv* env,
        jclass,
        jint line_index,
        jint npc,
        jint npc_position,
        jdouble appear_seconds,
        jdouble show_delay_seconds,
        jboolean fade_in,
        jboolean fade_out,
        jint portrait_width,
        jint portrait_height,
        jintArray portrait_pixels,
        jint name_width,
        jint name_height,
        jintArray name_pixels,
        jint text_width,
        jint text_height,
        jintArray text_pixels) {
    std::vector<std::uint32_t> portrait;
    std::vector<std::uint32_t> name;
    std::vector<std::uint32_t> text;
    if (!copy_pixels(env, portrait_pixels, portrait_width, portrait_height, &portrait) ||
            !copy_pixels(env, name_pixels, name_width, name_height, &name) ||
            !copy_pixels(env, text_pixels, text_width, text_height, &text)) {
        return JNI_FALSE;
    }
    return nevergone::tutorial_dialogue_runtime::upload_line(
        static_cast<int>(line_index),
        static_cast<int>(npc),
        static_cast<int>(npc_position),
        static_cast<double>(appear_seconds),
        static_cast<double>(show_delay_seconds),
        fade_in == JNI_TRUE,
        fade_out == JNI_TRUE,
        static_cast<int>(portrait_width),
        static_cast<int>(portrait_height),
        portrait.data(),
        portrait.size(),
        static_cast<int>(name_width),
        static_cast<int>(name_height),
        name.data(),
        name.size(),
        static_cast<int>(text_width),
        static_cast<int>(text_height),
        text.data(),
        text.size()) ? JNI_TRUE : JNI_FALSE;
}

extern "C" JNIEXPORT jboolean JNICALL
Java_org_nevergone_recomp_TutorialDialogueAssetLoader_nativeFinishAssets(JNIEnv*, jclass) {
    return nevergone::tutorial_dialogue_runtime::finish_assets() ? JNI_TRUE : JNI_FALSE;
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
            jstring_to_utf8(env, files_dir),
            jstring_to_utf8(env, key),
            static_cast<std::size_t>(language_column),
            &value) || value.empty()) {
        return nullptr;
    }
    return env->NewStringUTF(value.c_str());
}

extern "C" JNIEXPORT jstring JNICALL
Java_org_nevergone_recomp_TutorialDialogueAssetLoader_nativeCurrentRoleName(
        JNIEnv* env,
        jclass) {
    if (env == nullptr) return nullptr;
    const std::string role_name = nevergone::character_name_state::snapshot().role_name;
    return role_name.empty() ? nullptr : env->NewStringUTF(role_name.c_str());
}

extern "C" JNIEXPORT jint JNICALL
Java_org_nevergone_recomp_TutorialDialogueAssetLoader_nativeCurrentCareer(JNIEnv*, jclass) {
    const auto snapshot = nevergone::character_name_state::snapshot();
    if (snapshot.career < 1 || snapshot.career > 3) return 0;
    return static_cast<jint>(snapshot.career);
}
