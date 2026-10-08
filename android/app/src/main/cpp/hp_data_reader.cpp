#include "hp_data_reader.h"

#include <cstring>
#include <utility>

namespace nevergone::hp_data {

Reader::Reader(const std::uint8_t* data, std::size_t size) {
    if (data != nullptr && size != 0) bytes_.assign(data, data + size);
}

Reader::Reader(std::vector<std::uint8_t> data) : bytes_(std::move(data)) {}

std::size_t Reader::size() const {
    return bytes_.size();
}

bool Reader::read_bytes(std::size_t offset, std::size_t length, void* out) const {
    if (length == 0) return true;
    if (out == nullptr || offset > bytes_.size() || length > bytes_.size() - offset) return false;
    std::memcpy(out, bytes_.data() + offset, length);
    return true;
}

bool Reader::read_i32_le(std::size_t offset, std::int32_t* out) const {
    if (out == nullptr || offset > bytes_.size() || sizeof(std::uint32_t) > bytes_.size() - offset) {
        return false;
    }
    const auto* p = bytes_.data() + offset;
    const std::uint32_t value =
            static_cast<std::uint32_t>(p[0]) |
            (static_cast<std::uint32_t>(p[1]) << 8u) |
            (static_cast<std::uint32_t>(p[2]) << 16u) |
            (static_cast<std::uint32_t>(p[3]) << 24u);
    *out = static_cast<std::int32_t>(value);
    return true;
}

bool Reader::read_u32_le(std::size_t offset, std::uint32_t* out) const {
    if (out == nullptr || offset > bytes_.size() || sizeof(std::uint32_t) > bytes_.size() - offset) {
        return false;
    }
    const auto* p = bytes_.data() + offset;
    *out = static_cast<std::uint32_t>(p[0]) |
            (static_cast<std::uint32_t>(p[1]) << 8u) |
            (static_cast<std::uint32_t>(p[2]) << 16u) |
            (static_cast<std::uint32_t>(p[3]) << 24u);
    return true;
}

bool Reader::read_f32_le(std::size_t offset, float* out) const {
    if (out == nullptr) return false;
    std::uint32_t bits = 0;
    if (!read_u32_le(offset, &bits)) return false;
    static_assert(sizeof(bits) == sizeof(*out));
    std::memcpy(out, &bits, sizeof(bits));
    return true;
}

bool Reader::read_bool8(std::size_t offset, bool* out) const {
    if (out == nullptr || offset >= bytes_.size()) return false;
    *out = bytes_[offset] != 0;
    return true;
}

bool Reader::read_fixed_string(
        std::size_t offset,
        std::size_t field_length,
        std::string* out) const {
    if (out == nullptr || offset > bytes_.size() || field_length > bytes_.size() - offset) {
        return false;
    }
    const auto* begin = bytes_.data() + offset;
    std::size_t length = 0;
    while (length < field_length && begin[length] != 0) ++length;
    out->assign(reinterpret_cast<const char*>(begin), length);
    return true;
}

}  // namespace nevergone::hp_data
