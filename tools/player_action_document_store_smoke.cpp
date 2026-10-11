#include <cassert>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <vector>

#include "player_action_document_store.h"

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
    std::uint32_t raw = 0;
    std::memcpy(&raw, &value, sizeof(raw));
    append_u32(bytes, raw);
}

void append_empty_string(std::vector<std::uint8_t>* bytes) {
    append_i32(bytes, 0);
    bytes->push_back(0);
}

std::vector<std::uint8_t> valid_document() {
    std::vector<std::uint8_t> bytes;
    append_i32(&bytes, 0x68);
    append_f32(&bytes, 2.0f);
    append_f32(&bytes, 3.0f);
    append_u32(&bytes, 1u);

    append_i32(&bytes, 11);
    for (int i = 0; i < 12; ++i) append_f32(&bytes, static_cast<float>(i + 1));
    bytes.push_back(1);
    append_i32(&bytes, 22);
    append_f32(&bytes, 4.0f);
    append_empty_string(&bytes);
    append_empty_string(&bytes);
    append_empty_string(&bytes);

    append_i32(&bytes, 0);
    append_i32(&bytes, 0);
    for (int i = 0; i < 6; ++i) append_i32(&bytes, 0);
    append_i32(&bytes, 0);
    append_i32(&bytes, 1);
    append_i32(&bytes, 2);
    append_i32(&bytes, 3);
    append_i32(&bytes, 4);
    append_f32(&bytes, 0.25f);
    append_f32(&bytes, 0.5f);
    append_i32(&bytes, 2);
    return bytes;
}

void write_bytes(const std::filesystem::path& path, const std::vector<std::uint8_t>& bytes) {
    std::filesystem::create_directories(path.parent_path());
    std::ofstream output(path, std::ios::binary);
    output.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    assert(output.good());
}

}  // namespace

int main() {
    namespace store = nevergone::player_action_document_store;

    const auto root = std::filesystem::temp_directory_path() / "nevergone-player-action-store-smoke";
    std::filesystem::remove_all(root);
    const auto action = root / "assets" / "player" / "player_start.actData";
    write_bytes(action, valid_document());

    store::reset();
    assert(store::load_from_assets(root.string(), "player/player_start.actData"));
    auto state = store::snapshot();
    assert(state.status == store::LoadStatus::kReady);
    assert(state.load_attempt_count == 1u);
    assert(state.primary_record_count == 1u);
    assert(state.bytes_consumed == 156u);
    assert(state.trailing_bytes == 0u);
    const auto document = store::current_document();
    assert(document.has_value());
    assert(document->primary_records.size() == 1u);

    assert(!store::load_from_assets(root.string(), "../player_start.actData"));
    state = store::snapshot();
    assert(state.status == store::LoadStatus::kInvalidRelativePath);
    assert(!store::current_document().has_value());

    assert(!store::load_from_assets(root.string(), "player/missing.actData"));
    assert(store::snapshot().status == store::LoadStatus::kMissing);

    assert(!store::load_from_assets(root.string(), "player/player_start.actData", 8u));
    assert(store::snapshot().status == store::LoadStatus::kTooLarge);

    const auto bad = root / "assets" / "player" / "bad.actData";
    write_bytes(bad, {1u, 2u, 3u, 4u});
    assert(!store::load_from_assets(root.string(), "player/bad.actData"));
    assert(store::snapshot().status == store::LoadStatus::kParseFailed);

    store::reset();
    assert(store::snapshot().status == store::LoadStatus::kIdle);
    assert(!store::current_document().has_value());
    std::filesystem::remove_all(root);
    return 0;
}
