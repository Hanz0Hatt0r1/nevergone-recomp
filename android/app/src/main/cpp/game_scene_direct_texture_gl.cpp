#include "game_scene_direct_texture_gl.h"

#include <GLES2/gl2.h>

#include <cstddef>
#include <cstdint>
#include <vector>

namespace nevergone::game_scene_direct_texture_gl {
namespace {

class GlesBackend final : public game_scene_direct_texture_cache::Backend {
public:
    game_scene_direct_texture_cache::TextureHandle create_texture(
            int width,
            int height,
            const std::uint32_t* argb_pixels,
            std::size_t pixel_count) override {
        if (width <= 0 || height <= 0 || argb_pixels == nullptr) return 0;
        const std::size_t w = static_cast<std::size_t>(width);
        const std::size_t h = static_cast<std::size_t>(height);
        if (h != 0 && w > static_cast<std::size_t>(-1) / h) return 0;
        if (w * h != pixel_count) return 0;

        std::vector<std::uint8_t> rgba(pixel_count * 4u);
        for (std::size_t index = 0; index < pixel_count; ++index) {
            const std::uint32_t pixel = argb_pixels[index];
            rgba[index * 4u + 0u] = static_cast<std::uint8_t>((pixel >> 16u) & 0xffu);
            rgba[index * 4u + 1u] = static_cast<std::uint8_t>((pixel >> 8u) & 0xffu);
            rgba[index * 4u + 2u] = static_cast<std::uint8_t>(pixel & 0xffu);
            rgba[index * 4u + 3u] = static_cast<std::uint8_t>((pixel >> 24u) & 0xffu);
        }

        while (glGetError() != GL_NO_ERROR) {
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
                width,
                height,
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
        return static_cast<game_scene_direct_texture_cache::TextureHandle>(texture);
    }

    void destroy_texture(game_scene_direct_texture_cache::TextureHandle handle) override {
        if (handle == 0) return;
        const GLuint texture = static_cast<GLuint>(handle);
        glDeleteTextures(1, &texture);
    }
};

GlesBackend g_backend;

}  // namespace

void on_surface_created() {
    game_scene_direct_texture_cache::reset_for_new_context();
}

bool sync() {
    return game_scene_direct_texture_cache::sync(g_backend);
}

void clear() {
    game_scene_direct_texture_cache::clear(g_backend);
}

game_scene_direct_texture_cache::Snapshot snapshot() {
    return game_scene_direct_texture_cache::snapshot();
}

bool texture_for_sprite_command(
        std::size_t sprite_command_index,
        game_scene_direct_texture_cache::Texture* out) {
    return game_scene_direct_texture_cache::texture_for_sprite_command(
            sprite_command_index,
            out);
}

}  // namespace nevergone::game_scene_direct_texture_gl
