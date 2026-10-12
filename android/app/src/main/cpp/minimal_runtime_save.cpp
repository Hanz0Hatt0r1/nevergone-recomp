#include "minimal_runtime_save.h"

#include <cerrno>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <limits>
#include <sys/stat.h>

namespace nevergone::minimal_runtime_save {
namespace {

constexpr std::uint8_t kMagic[4] = {'N', 'G', 'S', 'V'};
constexpr std::size_t kHeaderBytes = 16u;
constexpr std::size_t kFixedPayloadBytes = 20u;
constexpr std::size_t kMaxEncodedBytes =
    kHeaderBytes + kFixedPayloadBytes + kMaxSceneGuidBytes;

void append_u32(std::vector<std::uint8_t>* out, std::uint32_t value) {
    out->push_back(static_cast<std::uint8_t>(value & 0xffu));
    out->push_back(static_cast<std::uint8_t>((value >> 8u) & 0xffu));
    out->push_back(static_cast<std::uint8_t>((value >> 16u) & 0xffu));
    out->push_back(static_cast<std::uint8_t>((value >> 24u) & 0xffu));
}

bool read_u32(
        const std::uint8_t* bytes,
        std::size_t size,
        std::size_t offset,
        std::uint32_t* value) {
    if (bytes == nullptr || value == nullptr || offset > size || size - offset < 4u) {
        return false;
    }
    *value = static_cast<std::uint32_t>(bytes[offset]) |
        (static_cast<std::uint32_t>(bytes[offset + 1u]) << 8u) |
        (static_cast<std::uint32_t>(bytes[offset + 2u]) << 16u) |
        (static_cast<std::uint32_t>(bytes[offset + 3u]) << 24u);
    return true;
}

void append_f32(std::vector<std::uint8_t>* out, float value) {
    std::uint32_t bits = 0u;
    static_assert(sizeof(bits) == sizeof(value));
    std::memcpy(&bits, &value, sizeof(bits));
    append_u32(out, bits);
}

bool read_f32(
        const std::uint8_t* bytes,
        std::size_t size,
        std::size_t offset,
        float* value) {
    std::uint32_t bits = 0u;
    if (value == nullptr || !read_u32(bytes, size, offset, &bits)) return false;
    std::memcpy(value, &bits, sizeof(bits));
    return true;
}

std::uint32_t checksum(const std::uint8_t* bytes, std::size_t size) {
    // Project-owned FNV-1a checksum. It detects corrupt/truncated sidecars; it is
    // not intended as authentication or as a claim about the original save.
    std::uint32_t value = 2166136261u;
    for (std::size_t index = 0; index < size; ++index) {
        value ^= bytes[index];
        value *= 16777619u;
    }
    return value;
}

bool safe_guid(const std::string& guid) {
    if (guid.empty() || guid.size() > kMaxSceneGuidBytes) return false;
    for (const char ch : guid) {
        if (ch == '\0' || ch == '\n' || ch == '\r') return false;
    }
    return true;
}

std::string state_directory(const std::string& files_dir) {
    if (files_dir.empty()) return {};
    return files_dir + "/nevergone-recomp";
}

}  // namespace

bool valid(const Record& record) {
    return record.player_index >= 1u && record.player_index <= 3u &&
        safe_guid(record.scene_guid) &&
        std::isfinite(record.motion_offset_x) &&
        std::isfinite(record.motion_offset_y) &&
        std::fabs(record.motion_offset_x) <= kMaxAbsMotionOffset &&
        std::fabs(record.motion_offset_y) <= kMaxAbsMotionOffset;
}

std::vector<std::uint8_t> encode(const Record& record) {
    if (!valid(record)) return {};

    std::vector<std::uint8_t> payload;
    payload.reserve(kFixedPayloadBytes + record.scene_guid.size());
    append_u32(&payload, record.scene_index);
    append_u32(&payload, record.player_index);
    append_f32(&payload, record.motion_offset_x);
    append_f32(&payload, record.motion_offset_y);
    append_u32(&payload, static_cast<std::uint32_t>(record.scene_guid.size()));
    payload.insert(payload.end(), record.scene_guid.begin(), record.scene_guid.end());

    std::vector<std::uint8_t> encoded;
    encoded.reserve(kHeaderBytes + payload.size());
    encoded.insert(encoded.end(), std::begin(kMagic), std::end(kMagic));
    append_u32(&encoded, kFormatVersion);
    append_u32(&encoded, static_cast<std::uint32_t>(payload.size()));
    append_u32(&encoded, checksum(payload.data(), payload.size()));
    encoded.insert(encoded.end(), payload.begin(), payload.end());
    return encoded;
}

bool decode(const std::uint8_t* bytes, std::size_t size, Record* output) {
    if (bytes == nullptr || output == nullptr || size < kHeaderBytes + kFixedPayloadBytes ||
        size > kMaxEncodedBytes ||
        std::memcmp(bytes, kMagic, sizeof(kMagic)) != 0) {
        return false;
    }

    std::uint32_t version = 0u;
    std::uint32_t payload_size = 0u;
    std::uint32_t expected_checksum = 0u;
    if (!read_u32(bytes, size, 4u, &version) || version != kFormatVersion ||
        !read_u32(bytes, size, 8u, &payload_size) ||
        !read_u32(bytes, size, 12u, &expected_checksum)) {
        return false;
    }
    if (payload_size != size - kHeaderBytes ||
        checksum(bytes + kHeaderBytes, payload_size) != expected_checksum) {
        return false;
    }

    const std::size_t base = kHeaderBytes;
    Record record;
    std::uint32_t guid_size = 0u;
    if (!read_u32(bytes, size, base + 0u, &record.scene_index) ||
        !read_u32(bytes, size, base + 4u, &record.player_index) ||
        !read_f32(bytes, size, base + 8u, &record.motion_offset_x) ||
        !read_f32(bytes, size, base + 12u, &record.motion_offset_y) ||
        !read_u32(bytes, size, base + 16u, &guid_size) ||
        guid_size == 0u || guid_size > kMaxSceneGuidBytes ||
        static_cast<std::size_t>(guid_size) !=
            payload_size - kFixedPayloadBytes) {
        return false;
    }

    record.scene_guid.assign(
        reinterpret_cast<const char*>(bytes + base + kFixedPayloadBytes),
        static_cast<std::size_t>(guid_size));
    if (!valid(record)) return false;
    *output = std::move(record);
    return true;
}

std::string save_path(const std::string& files_dir) {
    const std::string directory = state_directory(files_dir);
    return directory.empty() ? std::string{} : directory + "/minimal-save-v1.bin";
}

bool write_atomic(const std::string& files_dir, const Record& record) {
    const auto encoded = encode(record);
    const std::string directory = state_directory(files_dir);
    const std::string target = save_path(files_dir);
    if (encoded.empty() || directory.empty() || target.empty()) return false;

    if (::mkdir(directory.c_str(), 0700) != 0 && errno != EEXIST) return false;

    const std::string temporary = target + ".tmp";
    {
        std::ofstream stream(temporary, std::ios::binary | std::ios::trunc);
        if (!stream) return false;
        stream.write(
            reinterpret_cast<const char*>(encoded.data()),
            static_cast<std::streamsize>(encoded.size()));
        stream.flush();
        if (!stream) {
            stream.close();
            std::remove(temporary.c_str());
            return false;
        }
    }

    if (std::rename(temporary.c_str(), target.c_str()) != 0) {
        std::remove(temporary.c_str());
        return false;
    }
    return true;
}

bool read(const std::string& files_dir, Record* output) {
    if (output == nullptr) return false;
    const std::string path = save_path(files_dir);
    if (path.empty()) return false;

    std::ifstream stream(path, std::ios::binary | std::ios::ate);
    if (!stream) return false;
    const std::streamoff end = stream.tellg();
    if (end <= 0 || end > static_cast<std::streamoff>(kMaxEncodedBytes)) return false;
    stream.seekg(0, std::ios::beg);

    std::vector<std::uint8_t> bytes(static_cast<std::size_t>(end));
    stream.read(
        reinterpret_cast<char*>(bytes.data()),
        static_cast<std::streamsize>(bytes.size()));
    if (!stream || stream.gcount() != static_cast<std::streamsize>(bytes.size())) return false;
    return decode(bytes.data(), bytes.size(), output);
}

}  // namespace nevergone::minimal_runtime_save
