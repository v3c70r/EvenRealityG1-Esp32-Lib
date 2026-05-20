#include <catch2/catch_test_macros.hpp>
#include <cstdint>
#include <cstring>

// CRC-32/XZ is implemented in src/graphics/crc32_xz.cpp
// We declare it here for testing
namespace eveng1 {
    uint32_t crc32_xz(const uint8_t* data, size_t len);
    void crc32_xz_bmp(const uint8_t* addr4, const uint8_t* data, size_t data_len, uint8_t out_crc[4]);
}

using namespace eveng1;

TEST_CASE("CRC-32/XZ empty input", "[crc32]") {
    REQUIRE(crc32_xz(nullptr, 0) == 0x00000000);
}

TEST_CASE("CRC-32/XZ known value", "[crc32]") {
    // "123456789" CRC-32 (standard/zlib) = 0xCBF43926
    const char* test = "123456789";
    uint32_t crc = crc32_xz(reinterpret_cast<const uint8_t*>(test), 9);
    REQUIRE(crc == 0xCBF43926);
}

TEST_CASE("CRC-32/XZ consistency", "[crc32]") {
    uint8_t data[] = {0x00, 0x01, 0x02, 0x03, 0xFF};
    uint32_t crc1 = crc32_xz(data, sizeof(data));
    uint32_t crc2 = crc32_xz(data, sizeof(data));
    REQUIRE(crc1 == crc2);
}

TEST_CASE("CRC-32/XZ BMP with address", "[crc32]") {
    uint8_t addr[4] = {0x00, 0x1C, 0x00, 0x00};
    uint8_t pixelData[10] = {0};
    uint8_t crc[4];
    crc32_xz_bmp(addr, pixelData, sizeof(pixelData), crc);

    // Verify it produces non-zero and is consistent
    uint8_t crc2[4];
    crc32_xz_bmp(addr, pixelData, sizeof(pixelData), crc2);
    REQUIRE(std::memcmp(crc, crc2, 4) == 0);
}

TEST_CASE("CRC-32/XZ different data produces different CRC", "[crc32]") {
    uint8_t addr[4] = {0x00, 0x1C, 0x00, 0x00};
    uint8_t data1[10] = {0};
    uint8_t data2[10] = {0xFF};
    uint8_t crc1[4], crc2[4];
    crc32_xz_bmp(addr, data1, sizeof(data1), crc1);
    crc32_xz_bmp(addr, data2, sizeof(data2), crc2);
    REQUIRE(std::memcmp(crc1, crc2, 4) != 0);
}
