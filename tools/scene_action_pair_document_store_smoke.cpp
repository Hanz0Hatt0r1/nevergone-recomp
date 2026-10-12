#include <cassert>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <vector>

#include "scene_action_pair_document_store.h"

namespace store = nevergone::scene_action_pair_document_store;

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
    const auto root = std::filesystem::temp_directory_path() / "nevergone-scene-action-store-smoke";
    std::error_code error;
    std::filesystem::remove_all(root, error);
    std::filesystem::create_directories(root / "assets" / "scene", error);
    assert(!error);

    const auto document = empty_trap_document();
    write_file(root / "assets" / "scene" / "se01_born01.actData", document);
    write_file(root / "assets" / "scene" / "se01_i01.actData", document);
    write_file(root / "assets" / "scene" / "se01_born02.actData", document);

    store::reset();
    assert(store::load_from_assets(
            store::Slot::kBorn, 1, root.string(), "scene/se01_born01.actData"));
    auto state = store::snapshot();
    assert(state.variant == 1);
    assert(state.born.status == store::LoadStatus::kReady);
    assert(state.idle.status == store::LoadStatus::kIdle);
    assert(!state.pair_ready);

    assert(store::load_from_assets(
            store::Slot::kIdle, 1, root.string(), "scene/se01_i01.actData"));
    state = store::snapshot();
    assert(state.pair_ready);
    assert(store::current_document(store::Slot::kBorn).has_value());
    assert(store::current_document(store::Slot::kIdle).has_value());

    // A variant change invalidates the old pair before admitting a new slot.
    assert(store::load_from_assets(
            store::Slot::kBorn, 2, root.string(), "scene/se01_born02.actData"));
    state = store::snapshot();
    assert(state.variant == 2);
    assert(state.born.status == store::LoadStatus::kReady);
    assert(state.idle.status == store::LoadStatus::kIdle);
    assert(!state.pair_ready);
    assert(!store::current_document(store::Slot::kIdle).has_value());

    // Paths must remain beneath filesDir/assets.
    assert(!store::load_from_assets(
            store::Slot::kIdle, 2, root.string(), "../escape.actData"));
    assert(store::snapshot().idle.status == store::LoadStatus::kInvalidRelativePath);

    std::filesystem::remove_all(root, error);
    return 0;
}
