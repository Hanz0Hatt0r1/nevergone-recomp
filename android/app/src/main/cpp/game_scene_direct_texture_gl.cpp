#include "game_scene_direct_texture_gl.h"

#include <GLES2/gl2.h>

#include <cstddef>
#include <cstdint>
#include <vector>

#include "game_scene_png_premultiply.h"
#include "game_scene_rgba4444.h"

namespace nevergone::game_scene_direct_texture_gl {
namespace {

class GlesBackend final : public game_scene_direct_texture_cache::Backend {
public:
    game_scene_direct_texture_cache::TextureHandle create_texture(
            int width,
            int height,
            const std::uint32_t* argb_pixels,
            std::size_t pixel_count) override {
        return create_texture(
                game_scene_direct_texture_cache::PixelFormat::kRgba8888,
                width,
                height,
                argb_pixels,
                pixel_count);
    }

    game_scene_direct_texture_cache::TextureHandle create_texture(
            game_scene_direct_texture_cache::PixelFormat pixel_format,
            int width,
            int height,
            const std::uint32_t* argb_pixels,
            std::size_t pixel_count) override {
        if (width <= 0 || height <= 0 || argb_pixels == nullptr) return 0;
        const std::size_t w = static_cast<std::size_t>(width);
        const std::size_t h = static_cast<std::size_t>(height);
        if (h != 0 && w > static_cast<std::size_t>(-1) / h) return 0;
        if (w * h != pixel_count) return 0;

        std::vector<std::uint8_t> rgba8888;
        std::vector<std::uint16_t> rgba4444;
        const void* upload_data = nullptr;
        GLenum upload_type = 0;

        if (pixel_format == game_scene_direct_texture_cache::PixelFormat::kRgba8888) {
            rgba8888.resize(pixel_count * 4u);
            for (std::size_t index = 0; index < pixel_count; ++index) {
                const std::uint32_t pixel = argb_pixels[index];
                const std::uint8_t alpha =
                        static_cast<std::uint8_t>((pixel >> 24u) & 0xffu);
                rgba8888[index * 4u + 0u] = game_scene_png_premultiply::channel(
                        static_cast<std::uint8_t>((pixel >> 16u) & 0xffu), alpha);
                rgba8888[index * 4u + 1u] = game_scene_png_premultiply::channel(
                        static_cast<std::uint8_t>((pixel >> 8u) & 0xffu), alpha);
                rgba8888[index * 4u + 2u] = game_scene_png_premultiply::channel(
                        static_cast<std::uint8_t>(pixel & 0xffu), alpha);
                rgba8888[index * 4u + 3u] = alpha;
            }
            upload_data = rgba8888.data();
            upload_type = GL_UNSIGNED_BYTE;
        } else if (pixel_format == game_scene_direct_texture_cache::PixelFormat::kRgba4444) {
            rgba4444.resize(pixel_count);
            for (std::size_t index = 0; index < pixel_count; ++index) {
                const std::uint32_t pixel = argb_pixels[index];
                const std::uint8_t alpha =
                        static_cast<std::uint8_t>((pixel >> 24u) & 0xffu);
                const std::uint8_t red = game_scene_png_premultiply::channel(
                        static_cast<std::uint8_t>((pixel >> 16u) & 0xffu), alpha);
                const std::uint8_t green = game_scene_png_premultiply::channel(
                        static_cast<std::uint8_t>((pixel >> 8u) & 0xffu), alpha);
                const std::uint8_t blue = game_scene_png_premultiply::channel(
                        static_cast<std::uint8_t>(pixel & 0xffu), alpha);
                rgba4444[index] = game_scene_rgba4444::pack(red, green, blue, alpha);
            }
            upload_data = rgba4444.data();
            upload_type = GL_UNSIGNED_SHORT_4_4_4_4;
        } else {
            return 0;
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
                upload_type,
                upload_data);
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
    game_scene_direct_texture_gl::clear();
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
