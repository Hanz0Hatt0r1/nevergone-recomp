#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace nevergone::hp_data {

class Reader {
public:
    Reader() = default;
    Reader(const std::uint8_t* data, std::size_t size);
    explicit Reader(std::vector<std::uint8_t> data);

    std::size_t size() const;
    bool read_bytes(std::size_t offset, std::size_t length, void* out) const;

    bool read_i32_le(std::size_t offset, std::int32_t* out) const;
    bool read_u32_le(std::size_t offset, std::uint32_t* out) const;
    bool read_f32_le(std::size_t offset, float* out) const;
    bool read_bool8(std::size_t offset, bool* out) const;

    // Reads one fixed-width char field without assuming the unresolved HPRange
    // object layout. The returned string stops at the first NUL byte, matching
    // the observed char-buffer -> CCString::create call pattern.
    bool read_fixed_string(
            std::size_t offset,
            std::size_t field_length,
            std::string* out) const;

private:
    std::vector<std::uint8_t> bytes_;
};

}  // namespace nevergone::hp_data
