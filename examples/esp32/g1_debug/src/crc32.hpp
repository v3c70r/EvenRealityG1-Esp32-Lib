#pragma once
#include <cstdint>
#include <cstddef>

namespace eveng1 {

/// CRC-32/XZ (standard Ethernet CRC-32 with reflected I/O).
/// Polynomial: 0x04C11DB7, Init: 0xFFFFFFFF, XorOut: 0xFFFFFFFF
/// Test vector: "123456789" -> 0xCBF43926

uint32_t crc32_xz(const uint8_t* data, size_t len);

/// Compute CRC-32/XZ and return as 4-byte big-endian integer
inline uint32_t crc32_xz_be(const uint8_t* data, size_t len) {
    return crc32_xz(data, len);
}

/// Compute CRC-32/XZ over address + data, return 4 big-endian bytes
void crc32_xz_bmp(const uint8_t* addr4, const uint8_t* data, size_t data_len,
                  uint8_t out_crc[4]);

/// Compute CRC-32/XZ over buffer and write 4 big-endian bytes to out_crc
inline void crc32_xz(const uint8_t* data, size_t len, uint8_t out_crc[4]) {
    uint32_t val = crc32_xz(data, len);
    out_crc[0] = (val >> 24) & 0xFF;
    out_crc[1] = (val >> 16) & 0xFF;
    out_crc[2] = (val >> 8) & 0xFF;
    out_crc[3] = val & 0xFF;
}

}  // namespace eveng1
