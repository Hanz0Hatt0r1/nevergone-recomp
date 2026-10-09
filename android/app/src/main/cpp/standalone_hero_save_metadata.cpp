#include "standalone_hero_save_metadata.h"

#include <array>
#include <fstream>
#include <limits>
#include <sstream>
#include <utility>

namespace nevergone::standalone_hero_save_metadata {
namespace {

bool shipped_slot(std::uint32_t slot_id) {
    return slot_id == 1u || slot_id == 2u;
}

bool next_token(std::string_view bytes, std::size_t* cursor, std::string_view* output) {
    if (cursor == nullptr || output == nullptr || *cursor >= bytes.size()) return false;
    const std::size_t begin = *cursor;
    const std::size_t end = bytes.find(' ', begin);
    if (end == std::string_view::npos || end == begin) return false;
    *output = bytes.substr(begin, end - begin);
    *cursor = end + 1u;
    return true;
}

bool parse_i32(std::string_view token, std::int32_t* output) {
    if (output == nullptr || token.empty()) return false;
    bool negative = false;
    std::size_t index = 0;
    if (token.front() == '-') {
        negative = true;
        index = 1;
        if (index == token.size()) return false;
    } else if (token.front() == '+') {
        index = 1;
        if (index == token.size()) return false;
    }

    const std::int64_t limit = negative
        ? static_cast<std::int64_t>(std::numeric_limits<std::int32_t>::max()) + 1
        : static_cast<std::int64_t>(std::numeric_limits<std::int32_t>::max());
    std::int64_t value = 0;
    for (; index < token.size(); ++index) {
        const char ch = token[index];
        if (ch < '0' || ch > '9') return false;
        const std::int64_t digit = static_cast<std::int64_t>(ch - '0');
        if (value > (limit - digit) / 10) return false;
        value = value * 10 + digit;
    }
    if (negative) value = -value;
    *output = static_cast<std::int32_t>(value);
    return true;
}

bool next_i32(std::string_view bytes, std::size_t* cursor, std::int32_t* output) {
    std::string_view token;
    return next_token(bytes, cursor, &token) && parse_i32(token, output);
}

std::string save_path(const std::string& files_dir, std::uint32_t slot_id) {
    if (files_dir.empty() || !shipped_slot(slot_id)) return {};
    return files_dir + (slot_id == 1u ? "/DMG_01.sData" : "/DMG_02.sData");
}

}  // namespace

bool parse(std::string_view bytes, std::uint32_t slot_id, Metadata* output) {
    if (output == nullptr || !shipped_slot(slot_id) || bytes.empty()) return false;
    *output = {};

    // Save files are text-like token streams and may have a terminal NUL when
    // captured from legacy buffers. It is outside every space-delimited token.
    while (!bytes.empty() && bytes.back() == '\0') bytes.remove_suffix(1);

    std::size_t cursor = 0;
    std::int32_t magic = 0;
    if (!next_i32(bytes, &cursor, &magic) || magic != kSaveMagic) return false;

    std::string_view device_uuid;
    if (!next_token(bytes, &cursor, &device_uuid) || device_uuid.empty()) return false;

    std::int32_t version = 0;
    if (!next_i32(bytes, &cursor, &version) ||
            version <= kMinimumAcceptedVersionExclusive) {
        return false;
    }

    std::int32_t history_count = 0;
    if (!next_i32(bytes, &cursor, &history_count) ||
            history_count < 0 || history_count > kMaxDeviceHistory) {
        return false;
    }
    for (std::int32_t index = 0; index < history_count; ++index) {
        std::string_view historical_uuid;
        if (!next_token(bytes, &cursor, &historical_uuid) || historical_uuid.empty()) return false;
    }

    // LoadStandaloneHeroDataList reads these 13 integers into SaveDataHero at:
    // +40,+44,+D4,+48,+4C,+50,+54,+58,+5C,+60,+64,+68,+6C.
    std::array<std::int32_t, 13> hero_fields{};
    for (std::int32_t& value : hero_fields) {
        if (!next_i32(bytes, &cursor, &value)) return false;
    }

    Metadata parsed;
    parsed.slot_id = slot_id;
    parsed.version_code = version;
    parsed.level = hero_fields[5];          // SaveDataHero + 0x50
    parsed.game_hours = hero_fields[10];    // SaveDataHero + 0x64
    parsed.game_minutes = hero_fields[11];  // SaveDataHero + 0x68
    parsed.name_key = "Hero" + std::to_string(slot_id) + "Name";
    *output = std::move(parsed);
    return true;
}

bool read_file(const std::string& files_dir, std::uint32_t slot_id, Metadata* output) {
    if (output == nullptr) return false;
    const std::string path = save_path(files_dir, slot_id);
    if (path.empty()) return false;

    std::ifstream input(path, std::ios::binary);
    if (!input) return false;
    std::ostringstream bytes;
    bytes << input.rdbuf();
    if (!input.good() && !input.eof()) return false;
    return parse(bytes.str(), slot_id, output);
}

std::vector<Metadata> load_present(const std::string& files_dir) {
    std::vector<Metadata> result;
    for (const std::uint32_t slot_id : {1u, 2u}) {
        Metadata metadata;
        if (read_file(files_dir, slot_id, &metadata)) result.push_back(std::move(metadata));
    }
    return result;
}

std::string status_report(const std::string& files_dir) {
    const auto records = load_present(files_dir);
    std::ostringstream out;
    out << "Standalone hero metadata: " << records.size() << " valid save(s)\n";
    for (const Metadata& record : records) {
        out << "  slot=" << record.slot_id
            << " version=" << record.version_code
            << " level=" << record.level
            << " play=" << record.game_hours << ":" << record.game_minutes
            << " name-key=" << record.name_key << "\n";
    }
    return out.str();
}

}  // namespace nevergone::standalone_hero_save_metadata
