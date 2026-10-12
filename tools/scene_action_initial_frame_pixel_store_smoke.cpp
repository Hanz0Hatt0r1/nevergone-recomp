#include <array>
#include <cassert>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <vector>

#include "scene_action_initial_frame_pixel_store.h"
#include "scene_action_pair_document_store.h"

namespace docs = nevergone::scene_action_pair_document_store;
namespace pixels = nevergone::scene_action_initial_frame_pixel_store;

namespace {

void append_u32(std::vector<std::uint8_t>* bytes, std::uint32_t value) {
    bytes->push_back(static_cast<std::uint8_t>(value & 0xffu));
    bytes->push_back(static_cast<std::uint8_t>((value >> 8u) & 0xffu));
    bytes->push_back(static_cast<std::uint8_t>((value >> 16u) & 0xffu));
    bytes->push_back(static_cast<std::uint8_t>((value >> 24u) & 0xffu));
}

void append_i32(std::vector<std::uint8_t>* bytes, std::int32_t value) {
    append_u32(bytes, static_cast<std::uint32_t>(value));
}

void append_f32(std::vector<std::uint8_t>* bytes, float value) {
    std::uint32_t bits = 0;
    std::memcpy(&bits, &value, sizeof(bits));
    append_u32(bytes, bits);
}

std::vector<std::uint8_t> empty_trap_document() {
    std::vector<std::uint8_t> bytes;
    append_i32(&bytes, 0x68);
    append_f32(&bytes, 1.0f);
    append_f32(&bytes, 2.0f);
    append_u32(&bytes, 0u);
    append_u32(&bytes, 0u);
    append_i32(&bytes, 0);
    for (int i = 0; i < 6; ++i) append_i32(&bytes, 0);
    append_i32(&bytes, 0);
    return bytes;
}

void write_file(const std::filesystem::path& path, const std::vector<std::uint8_t>& bytes) {
    std::ofstream out(path, std::ios::binary);
    assert(out);
    out.write(reinterpret_cast<const char*>(bytes.data()),
              static_cast<std::streamsize>(bytes.size()));
    assert(out);
}

}  // namespace

int main() {
    const auto root = std::filesystem::temp_directory_path() /
            "nevergone-scene-action-pixel-store-smoke";
    std::error_code error;
    std::filesystem::remove_all(root, error);
    std::filesystem::create_directories(root / "assets" / "scene", error);
    assert(!error);

    const auto document = empty_trap_document();
    write_file(root / "assets" / "scene" / "se01_born01.actData", document);
    write_file(root / "assets" / "scene" / "se01_i01.actData", document);

    docs::reset();
    pixels::reset();
    const std::array<std::uint32_t, 4> argb = {
            0xff010203u, 0xff040506u, 0xff070809u, 0xff0a0b0cu};

    assert(!pixels::upload(docs::Slot::kBorn, 1, 0u, 2, 2, argb.data(), argb.size()));

    assert(docs::load_from_assets(
            docs::Slot::kBorn, 1, root.string(), "scene/se01_born01.actData"));
    assert(docs::load_from_assets(
            docs::Slot::kIdle, 1, root.string(), "scene/se01_i01.actData"));
    assert(docs::snapshot().pair_ready);

    assert(pixels::upload(docs::Slot::kBorn, 1, 0u, 2, 2, argb.data(), argb.size()));
    assert(pixels::upload(docs::Slot::kIdle, 1, 1u, 2, 2, argb.data(), argb.size()));
    assert(pixels::ready(docs::Slot::kBorn, 1));
    assert(pixels::ready(docs::Slot::kIdle, 1));
    assert(pixels::ready_frame(docs::Slot::kBorn, 1, 0u));
    assert(!pixels::ready_frame(docs::Slot::kBorn, 1, 1u));

    const auto born = pixels::current(docs::Slot::kBorn);
    assert(born.has_value());
    assert(born->variant == 1);
    assert(born->mode_24e == 0u);
    assert(born->frame_index == 0u);
    assert(born->width == 2 && born->height == 2);
    assert(born->argb_pixels == std::vector<std::uint32_t>(argb.begin(), argb.end()));

    // Explicit later-frame upload is document-bounded; these synthetic smoke
    // documents intentionally contain no primary ActionFrameData records.
    assert(!pixels::upload_frame(
            docs::Slot::kBorn, 1, 0u, 0u, 2, 2, argb.data(), argb.size()));

    assert(!pixels::upload(docs::Slot::kBorn, 1, 1u, 2, 2, argb.data(), argb.size()));
    assert(!pixels::upload(docs::Slot::kBorn, 2, 0u, 2, 2, argb.data(), argb.size()));
    assert(!pixels::upload(docs::Slot::kBorn, 1, 0u, 2, 3, argb.data(), argb.size()));

    pixels::reset();
    assert(!pixels::ready(docs::Slot::kBorn, 1));
    assert(!pixels::current(docs::Slot::kIdle).has_value());

    std::filesystem::remove_all(root, error);
    return 0;
}
