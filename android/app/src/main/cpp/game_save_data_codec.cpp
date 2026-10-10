#include "game_save_data_codec.h"

namespace nevergone::game_save_data_codec {
namespace {

constexpr std::uint32_t kCounterModulus = 0x7fu;

std::uint8_t encode_byte(std::uint8_t value, std::uint32_t counter, std::uint32_t key) {
    return static_cast<std::uint8_t>((static_cast<std::uint32_t>(value) + counter) ^ key);
}

std::uint8_t decode_byte(std::uint8_t value, std::uint32_t counter, std::uint32_t key) {
    return static_cast<std::uint8_t>((static_cast<std::uint32_t>(value) ^ key) - counter);
}

template <typename Transform>
std::size_t transform(
    const std::uint8_t* source,
    std::size_t length,
    std::uint8_t* destination,
    std::uint32_t key,
    Transform transform_byte) {
    if (length == 0u) return 0u;
    if (source == nullptr || destination == nullptr) return 0u;

    std::uint32_t counter = 0u;
    for (std::size_t index = 0; index < length; ++index) {
        destination[index] = transform_byte(source[index], counter, key);
        ++counter;
        if (counter == kCounterModulus) counter = 0u;
    }
    return length;
}

}  // namespace

std::size_t encode(
    const std::uint8_t* source,
    std::size_t length,
    std::uint8_t* destination,
    std::uint32_t key) {
    return transform(source, length, destination, key, encode_byte);
}

std::size_t decode(
    const std::uint8_t* source,
    std::size_t length,
    std::uint8_t* destination,
    std::uint32_t key) {
    return transform(source, length, destination, key, decode_byte);
}

std::vector<std::uint8_t> encode_copy(
    const std::vector<std::uint8_t>& source,
    std::uint32_t key) {
    std::vector<std::uint8_t> output(source.size());
    encode(source.data(), source.size(), output.data(), key);
    return output;
}

std::vector<std::uint8_t> decode_copy(
    const std::vector<std::uint8_t>& source,
    std::uint32_t key) {
    std::vector<std::uint8_t> output(source.size());
    decode(source.data(), source.size(), output.data(), key);
    return output;
}

}  // namespace nevergone::game_save_data_codec
