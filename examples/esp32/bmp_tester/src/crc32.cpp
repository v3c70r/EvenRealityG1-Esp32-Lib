#include "crc32.hpp"

namespace eveng1 {

static constexpr uint32_t CRC32_POLY = 0x04C11DB7;
static constexpr uint32_t CRC32_INIT = 0xFFFFFFFF;
static constexpr uint32_t CRC32_XOROUT = 0xFFFFFFFF;

static uint32_t reflect(uint32_t val, int bits) {
    uint32_t result = 0;
    for (int i = 0; i < bits; ++i) {
        if (val & (1 << i)) {
            result |= (1 << (bits - 1 - i));
        }
    }
    return result & ((1UL << bits) - 1);
}

uint32_t crc32_xz(const uint8_t* data, size_t len) {
    uint32_t crc = CRC32_INIT;

    for (size_t i = 0; i < len; ++i) {
        uint32_t reflected = reflect(data[i], 8);
        crc ^= reflected << 24;

        for (int b = 0; b < 8; ++b) {
            if (crc & 0x80000000) {
                crc = ((crc << 1) ^ CRC32_POLY) & 0xFFFFFFFF;
            } else {
                crc = (crc << 1) & 0xFFFFFFFF;
            }
        }
    }

    crc = reflect(crc, 32) ^ CRC32_XOROUT;
    return crc & 0xFFFFFFFF;
}

void crc32_xz_bmp(const uint8_t* addr4, const uint8_t* data, size_t data_len,
                  uint8_t out_crc[4]) {
    // Compute CRC over address + data
    // Build combined buffer (max ~10KB, acceptable on stack for ESP32)
    size_t total = 4 + data_len;
    uint8_t* buf = new uint8_t[total];
    buf[0] = addr4[0]; buf[1] = addr4[1]; buf[2] = addr4[2]; buf[3] = addr4[3];
    for (size_t i = 0; i < data_len; ++i) buf[4 + i] = data[i];

    uint32_t val = crc32_xz(buf, total);
    delete[] buf;

    // Big-endian output
    out_crc[0] = (val >> 24) & 0xFF;
    out_crc[1] = (val >> 16) & 0xFF;
    out_crc[2] = (val >> 8) & 0xFF;
    out_crc[3] = val & 0xFF;
}

}  // namespace eveng1
