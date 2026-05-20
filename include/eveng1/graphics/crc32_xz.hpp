#pragma once

#include <cstdint>
#include <cstddef>

namespace eveng1 {

/// Calculate CRC-32/XZ checksum over data.
/// Returns CRC value (standard CRC-32, same as zlib.crc32).
uint32_t crc32_xz(const uint8_t* data, size_t len);

/// Calculate CRC-32/XZ for BMP transfer.
/// CRC is computed over address (4 bytes) + pixel data.
/// Output is big-endian 4-byte array.
void crc32_xz_bmp(const uint8_t* addr4, const uint8_t* data, size_t data_len,
                  uint8_t out_crc[4]);

}  // namespace eveng1
