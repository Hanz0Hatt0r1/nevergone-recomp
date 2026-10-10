#include <array>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <vector>

#include "game_save_data_codec.h"

namespace {

std::vector<std::uint8_t> make_source(std::size_t length) {
    std::vector<std::uint8_t> source(length);
    for (std::size_t index = 0; index < length; ++index) {
        source[index] = static_cast<std::uint8_t>((index * 73u + 19u) & 0xffu);
    }
    return source;
}

std::vector<std::uint8_t> expected_encode(
    const std::vector<std::uint8_t>& source,
    std::uint32_t key) {
    std::vector<std::uint8_t> output(source.size());
    std::uint32_t counter = 0u;
    for (std::size_t index = 0; index < source.size(); ++index) {
        output[index] = static_cast<std::uint8_t>(
            (static_cast<std::uint32_t>(source[index]) + counter) ^ key);
        ++counter;
        if (counter == 0x7fu) counter = 0u;
    }
    return output;
}

std::vector<std::uint8_t> expected_decode(
    const std::vector<std::uint8_t>& source,
    std::uint32_t key) {
    std::vector<std::uint8_t> output(source.size());
    std::uint32_t counter = 0u;
    for (std::size_t index = 0; index < source.size(); ++index) {
        output[index] = static_cast<std::uint8_t>(
            (static_cast<std::uint32_t>(source[index]) ^ key) - counter);
        ++counter;
        if (counter == 0x7fu) counter = 0u;
    }
    return output;
}

void check_encode(std::size_t length, std::uint32_t key) {
    using nevergone::game_save_data_codec::encode;

    const std::vector<std::uint8_t> source = make_source(length);
    const std::vector<std::uint8_t> original = source;
    const std::vector<std::uint8_t> expected = expected_encode(source, key);

    std::vector<std::uint8_t> destination(length + 1u, 0u);
    destination[length] = 0x5au;
    const std::size_t result = encode(
        source.data(), source.size(), destination.data(), key);

    assert(result == length);
    assert(source == original);
    assert(destination[length] == 0x5au);
    assert(std::vector<std::uint8_t>(destination.begin(), destination.begin() + length) == expected);
}

void check_decode(std::size_t length, std::uint32_t key) {
    using nevergone::game_save_data_codec::decode;

    const std::vector<std::uint8_t> source = make_source(length);
    const std::vector<std::uint8_t> original = source;
    const std::vector<std::uint8_t> expected = expected_decode(source, key);

    std::vector<std::uint8_t> destination(length + 1u, 0u);
    destination[length] = 0x5au;
    const std::size_t result = decode(
        source.data(), source.size(), destination.data(), key);

    assert(result == length);
    assert(source == original);
    assert(destination[length] == 0x5au);
    assert(std::vector<std::uint8_t>(destination.begin(), destination.begin() + length) == expected);
}

}  // namespace

int main() {
    constexpr std::array<std::size_t, 6> kNativeProbeLengths{{0u, 1u, 126u, 127u, 128u, 255u}};
    constexpr std::array<std::uint32_t, 3> kNativeProbeKeys{{0u, 1u, 0x123u}};

    for (const std::size_t length : kNativeProbeLengths) {
        for (const std::uint32_t key : kNativeProbeKeys) {
            check_encode(length, key);
            check_decode(length, key);
        }
    }

    // Derived inverse-property check on the same bounded domain. The 36
    // direct expected-value checks above are the authoritative native-backed
    // regression; this additionally protects the paired project API.
    for (const std::size_t length : kNativeProbeLengths) {
        const auto source = make_source(length);
        for (const std::uint32_t key : kNativeProbeKeys) {
            const auto encoded = nevergone::game_save_data_codec::encode_copy(source, key);
            const auto decoded = nevergone::game_save_data_codec::decode_copy(encoded, key);
            assert(decoded == source);
        }
    }

    // Project safety behavior outside the native evidence boundary.
    std::uint8_t byte = 0u;
    assert(nevergone::game_save_data_codec::encode(nullptr, 1u, &byte, 1u) == 0u);
    assert(nevergone::game_save_data_codec::decode(&byte, 1u, nullptr, 1u) == 0u);

    return 0;
}
