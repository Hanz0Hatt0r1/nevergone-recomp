#include "scene_actions_gles_compositor.h"

#include <GLES2/gl2.h>

#include <algorithm>
#include <array>
#include <mutex>
#include <sstream>
#include <vector>

#include "offline_startup_flow.h"
#include "scene_actions_draw_geometry.h"

namespace nevergone::scene_actions_gles_compositor {
namespace {

constexpr float kDesignWidth = 1136.0f;
constexpr float kDesignHeight = 640.0f;
constexpr std::size_t kSlotCount = 2u;

struct Submission {
    bool active = false;
    scene_actions_render_frame_plan::Plan plan{};
    scene_action_initial_frame_pixel_store::Asset asset{};
    float parent_x = 0.0f;
    float parent_y = 0.0f;
    int child_z_order = 0;
    GLuint texture = 0;
    std::uint64_t uploaded_generation = 0;
};

std::mutex g_mutex;
std::array<Submission, kSlotCount> g_slots{};
GLuint g_program = 0;
GLint g_sampler = -1;
int g_surface_width = 0;
int g_surface_height = 0;
std::uint64_t g_upload_count = 0;
std::uint64_t g_draw_count = 0;

std::size_t slot_index(scene_action_pair_document_store::Slot slot) {
    return slot == scene_action_pair_document_store::Slot::kBorn ? 0u : 1u;
}

GLuint compile(GLenum type, const char* source) {
    const GLuint shader = glCreateShader(type);
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
    static constexpr const char* kVertex = R"(
attribute vec2 aPosition;
attribute vec2 aTexCoord;
varying vec2 vTexCoord;
void main() {
    vTexCoord = aTexCoord;
    gl_Position = vec4(aPosition, 0.0, 1.0);
})";
    static constexpr const char* kFragment = R"(
precision mediump float;
varying vec2 vTexCoord;
uniform sampler2D uTexture;
void main() {
    gl_FragColor = texture2D(uTexture, vTexCoord);
})";
    const GLuint vs = compile(GL_VERTEX_SHADER, kVertex);
    const GLuint fs = compile(GL_FRAGMENT_SHADER, kFragment);
    if (vs == 0 || fs == 0) {
        if (vs != 0) glDeleteShader(vs);
        if (fs != 0) glDeleteShader(fs);
        return 0;
    }
    const GLuint program = glCreateProgram();
    if (program == 0) {
        glDeleteShader(vs);
        glDeleteShader(fs);
        return 0;
    }
    glAttachShader(program, vs);
    glAttachShader(program, fs);
    glBindAttribLocation(program, 0, "aPosition");
    glBindAttribLocation(program, 1, "aTexCoord");
    glLinkProgram(program);
    glDeleteShader(vs);
    glDeleteShader(fs);
    GLint ok = GL_FALSE;
    glGetProgramiv(program, GL_LINK_STATUS, &ok);
    if (ok != GL_TRUE) {
        glDeleteProgram(program);
        return 0;
    }
    return program;
}

void delete_texture(Submission* slot) {
    if (slot == nullptr) return;
    if (slot->texture != 0) glDeleteTextures(1, &slot->texture);
    slot->texture = 0;
    slot->uploaded_generation = 0;
}

bool upload_if_needed(Submission* slot) {
    if (slot == nullptr || !slot->active) return false;
    if (slot->texture != 0 && slot->uploaded_generation == slot->asset.generation) return true;
    if (slot->asset.width <= 0 || slot->asset.height <= 0 || slot->asset.argb_pixels.empty()) return false;

    std::vector<std::uint8_t> rgba(slot->asset.argb_pixels.size() * 4u);
    for (std::size_t i = 0; i < slot->asset.argb_pixels.size(); ++i) {
        const std::uint32_t p = slot->asset.argb_pixels[i];
        rgba[i * 4u + 0u] = static_cast<std::uint8_t>((p >> 16u) & 0xffu);
        rgba[i * 4u + 1u] = static_cast<std::uint8_t>((p >> 8u) & 0xffu);
        rgba[i * 4u + 2u] = static_cast<std::uint8_t>(p & 0xffu);
        rgba[i * 4u + 3u] = static_cast<std::uint8_t>((p >> 24u) & 0xffu);
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
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, slot->asset.width, slot->asset.height, 0,
                 GL_RGBA, GL_UNSIGNED_BYTE, rgba.data());
    const GLenum error = glGetError();
    glBindTexture(GL_TEXTURE_2D, 0);
    if (error != GL_NO_ERROR) {
        glDeleteTextures(1, &texture);
        return false;
    }

    delete_texture(slot);
    slot->texture = texture;
    slot->uploaded_generation = slot->asset.generation;
    ++g_upload_count;
    return true;
}

}  // namespace

void on_surface_created() {
    std::lock_guard<std::mutex> lock(g_mutex);
    for (auto& slot : g_slots) delete_texture(&slot);
    if (g_program != 0) glDeleteProgram(g_program);
    g_program = build_program();
    g_sampler = g_program != 0 ? glGetUniformLocation(g_program, "uTexture") : -1;
    g_upload_count = 0;
    g_draw_count = 0;
}

void on_surface_changed(int width, int height) {
    std::lock_guard<std::mutex> lock(g_mutex);
    g_surface_width = width;
    g_surface_height = height;
}

bool submit(
        const scene_actions_render_frame_plan::Plan& plan,
        const scene_action_initial_frame_pixel_store::Asset& asset,
        float parent_x,
        float parent_y,
        int child_z_order) {
    if (plan.variant != asset.variant || plan.slot != asset.slot ||
            plan.mode_24e != asset.mode_24e || plan.frame_index != asset.frame_index ||
            plan.pixel_generation != asset.generation || plan.width != asset.width ||
            plan.height != asset.height || plan.pixel_count != asset.argb_pixels.size()) {
        return false;
    }
    if (!scene_actions_draw_geometry::build(plan, parent_x, parent_y, kDesignWidth, kDesignHeight).has_value()) {
        return false;
    }

    std::lock_guard<std::mutex> lock(g_mutex);
    Submission& target = g_slots[slot_index(plan.slot)];
    if (target.uploaded_generation != asset.generation) delete_texture(&target);
    target.active = true;
    target.plan = plan;
    target.asset = asset;
    target.parent_x = parent_x;
    target.parent_y = parent_y;
    target.child_z_order = child_z_order;
    return true;
}

void clear(scene_action_pair_document_store::Slot slot) {
    std::lock_guard<std::mutex> lock(g_mutex);
    Submission& target = g_slots[slot_index(slot)];
    delete_texture(&target);
    target = Submission{};
}

void reset() {
    std::lock_guard<std::mutex> lock(g_mutex);
    for (auto& slot : g_slots) {
        delete_texture(&slot);
        slot = Submission{};
    }
}

bool draw() {
    if (offline_startup_flow::snapshot().route != offline_startup_flow::Route::kGameLevels) return false;

    std::lock_guard<std::mutex> lock(g_mutex);
    if (g_program == 0 || g_sampler < 0 || g_surface_width <= 0 || g_surface_height <= 0) return false;

    std::array<std::size_t, kSlotCount> order = {0u, 1u};
    std::sort(order.begin(), order.end(), [](std::size_t a, std::size_t b) {
        return g_slots[a].child_z_order < g_slots[b].child_z_order;
    });

    bool drew = false;
    glUseProgram(g_program);
    glActiveTexture(GL_TEXTURE0);
    glUniform1i(g_sampler, 0);
    glEnableVertexAttribArray(0);
    glEnableVertexAttribArray(1);

    for (const std::size_t index : order) {
        Submission& slot = g_slots[index];
        if (!slot.active || !upload_if_needed(&slot)) continue;
        const auto quad = scene_actions_draw_geometry::build(
                slot.plan, slot.parent_x, slot.parent_y, kDesignWidth, kDesignHeight);
        if (!quad.has_value()) continue;
        glBindTexture(GL_TEXTURE_2D, slot.texture);
        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, quad->positions.data());
        glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 0, quad->texcoords.data());
        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
        drew = true;
    }

    glDisableVertexAttribArray(1);
    glDisableVertexAttribArray(0);
    glBindTexture(GL_TEXTURE_2D, 0);
    if (drew) ++g_draw_count;
    return drew;
}

std::string status_report() {
    std::lock_guard<std::mutex> lock(g_mutex);
    std::ostringstream out;
    out << "SceneActions GLES compositor\n";
    out << "program: " << (g_program != 0 ? "ready" : "unavailable") << "\n";
    out << "surface: " << g_surface_width << "x" << g_surface_height << "\n";
    out << "uploads: " << g_upload_count << "\n";
    out << "draws: " << g_draw_count << "\n";
    out << "born active: " << (g_slots[0].active ? "yes" : "no") << "\n";
    out << "idle active: " << (g_slots[1].active ? "yes" : "no") << "\n";
    return out.str();
}

}  // namespace nevergone::scene_actions_gles_compositor
