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

Cursor::Cursor(const Reader& reader, std::size_t offset)
        : reader_(&reader), offset_(offset <= reader.size() ? offset : reader.size()) {}

std::size_t Cursor::offset() const {
    return offset_;
}

std::size_t Cursor::remaining() const {
    return reader_ != nullptr && offset_ <= reader_->size() ? reader_->size() - offset_ : 0;
}

bool Cursor::seek(std::size_t offset) {
    if (reader_ == nullptr || offset > reader_->size()) return false;
    offset_ = offset;
    return true;
}

bool Cursor::skip(std::size_t length) {
    if (length > remaining()) return false;
    offset_ += length;
    return true;
}

bool Cursor::read_bytes(std::size_t length, void* out) {
    if (reader_ == nullptr || !reader_->read_bytes(offset_, length, out)) return false;
    offset_ += length;
    return true;
}

bool Cursor::read_i32_le(std::int32_t* out) {
    if (reader_ == nullptr || !reader_->read_i32_le(offset_, out)) return false;
    offset_ += sizeof(std::int32_t);
    return true;
}

bool Cursor::read_u32_le(std::uint32_t* out) {
    if (reader_ == nullptr || !reader_->read_u32_le(offset_, out)) return false;
    offset_ += sizeof(std::uint32_t);
    return true;
}

bool Cursor::read_f32_le(float* out) {
    if (reader_ == nullptr || !reader_->read_f32_le(offset_, out)) return false;
    offset_ += sizeof(float);
    return true;
}

bool Cursor::read_bool8(bool* out) {
    if (reader_ == nullptr || !reader_->read_bool8(offset_, out)) return false;
    ++offset_;
    return true;
}

bool Cursor::read_fixed_string(std::size_t field_length, std::string* out) {
    if (reader_ == nullptr || !reader_->read_fixed_string(offset_, field_length, out)) return false;
    offset_ += field_length;
    return true;
}

}  // namespace nevergone::hp_data
