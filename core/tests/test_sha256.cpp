// Tests for core/util/sha256: the FIPS 180-4 examples, plus lengths around the 64-byte block and
// its 56-byte padding boundary. The expected digests come from Python's hashlib (for the generated
// inputs, byte i is (7i + 3) mod 256).

#include "util/sha256.h"

#include <catch2/catch_test_macros.hpp>

#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

using namespace wubansia;

namespace
{
    std::string hashOf (std::string_view text)
    {
        const std::vector<std::uint8_t> bytes (text.begin(), text.end());
        return util::sha256Hex (bytes);
    }

    std::vector<std::uint8_t> pattern (std::size_t n)
    {
        std::vector<std::uint8_t> out (n);
        for (std::size_t i = 0; i < n; ++i)
            out[i] = static_cast<std::uint8_t> ((i * 7 + 3) & 0xFF);
        return out;
    }
}

TEST_CASE ("sha256: the FIPS 180-4 examples", "[util][sha256]")
{
    CHECK (hashOf ("") == "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");
    CHECK (hashOf ("abc") == "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
    CHECK (hashOf ("abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq")
           == "248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1");
    CHECK (hashOf ("abcdefghbcdefghicdefghijdefghijkefghijklfghijklmghijklmnhijklmnoijklmnopjklmnopqklmnopqrlmnopqrsmnopqrstnopqrstu")
           == "cf5b16a778af8380036ce59e7b0492370b249b11e8f07a51afac45037afee9d1");
    CHECK (hashOf (std::string (1000000, 'a')) == "cdc76e5c9914fb9281a1c7e284d73e67f1809a48a497200e046d39ccc7112cd0");
}

TEST_CASE ("sha256: lengths around the block and padding boundaries", "[util][sha256]")
{
    const std::vector<std::pair<std::size_t, std::string_view>> cases = {
        { 1, "084fed08b978af4d7d196a7446a86b58009e636b611db16211b65a9aadff29c5" },
        { 55, "e7313d333c272e639f790978283f9eb392e843d0f29b7016828bb1daa4aac70b" },
        { 56, "4324d65f3c103567f5589c710bc08f8523f929a9272e3af36fc968e52abc6c27" },
        { 57, "35df609437dcfea3279283ab79fd554e2bf78f8f7ae2de532d8ee300b09e8f73" },
        { 63, "81c80242132f230c3bd41b3e63bbcff16107339549214a99614ff26664625055" },
        { 64, "39e3d7b6b5d075d37d053ad89b24b41bef4f3c29760c84447cab3f3be1882241" },
        { 65, "aacca6ff74fdbb296d165a45cecfa04e5127bc008770fbbdd48006f2d2fae95e" },
        { 119, "9ce7368e4daf32341631b492e80359dc9f594b48453cd0dd5bf0b19279cc177e" },
        { 120, "7836b787757e95e58b3ca5aec90b1b004e8deba1e50e9675af9cabf1a13a04b5" },
        { 127, "a8d23e75d936f303d248888d9b165ee543f4cbafcad3c9dd2a79bd84faa11d07" },
        { 128, "d2742f1f4ac6bb7ca2b239ee18402ba8b3f9f8e652d2a72973c2b9ba11c08cf6" },
        { 129, "307f8fc2c1622b92762e818d39a185d4d667ad49a4b07ceae1f4afa008a93ec4" },
        { 1000, "1e9bc38cbf860b9ec31918b065f9b52476c549a782e0e7990bed8ce3868d2371" },
    };
    for (const auto& [n, digest] : cases)
    {
        INFO (n << " bytes");
        CHECK (util::sha256Hex (pattern (n)) == digest);
    }
}

TEST_CASE ("sha256: a view into a larger buffer hashes only its own bytes", "[util][sha256]")
{
    const auto all = pattern (200);
    const std::span<const std::uint8_t> middle (all.data() + 10, 65);
    const std::vector<std::uint8_t> copy (middle.begin(), middle.end());
    CHECK (util::sha256Hex (middle) == util::sha256Hex (copy));
    CHECK (util::sha256Hex (middle) != util::sha256Hex (pattern (65)));
}
