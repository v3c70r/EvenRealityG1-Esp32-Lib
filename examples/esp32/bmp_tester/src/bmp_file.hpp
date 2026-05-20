#pragma once
#include <cstdint>
#include <cstddef>

namespace eveng1 {

// BMP file header (14 bytes)
#pragma pack(push, 1)
struct BmpFileHeader {
    uint16_t signature;      // 'BM' = 0x4D42
    uint32_t fileSize;       // Total file size
    uint16_t reserved1;
    uint16_t reserved2;
    uint32_t dataOffset;     // Offset to pixel data
};

// DIB header (BITMAPINFOHEADER, 40 bytes)
struct BmpInfoHeader {
    uint32_t headerSize;     // 40
    int32_t  width;
    int32_t  height;
    uint16_t planes;         // 1
    uint16_t bitCount;       // 1 for monochrome
    uint32_t compression;    // 0 = none
    uint32_t imageSize;      // Can be 0 for uncompressed
    int32_t  xPixelsPerMeter;
    int32_t  yPixelsPerMeter;
    uint32_t colorsUsed;
    uint32_t colorsImportant;
};
#pragma pack(pop)

// Full BMP file for 576x136 1-bit
constexpr size_t BMP_HEADER_SIZE = 14 + 40 + 8; // File + Info + Color table
constexpr size_t BMP_ROW_BYTES = (576 + 31) / 32 * 4; // Padded to 4 bytes = 72
constexpr size_t BMP_PIXEL_SIZE = BMP_ROW_BYTES * 136;
constexpr size_t BMP_FULL_SIZE = BMP_HEADER_SIZE + BMP_PIXEL_SIZE;

// Generate a 576x136 1-bit BMP file in memory
void generateBmpFile(uint8_t* bmp, bool checkerboard);

// Packet size for BMP transfer (from reference app)
constexpr size_t BMP_PACKET_DATA_SIZE = 194;

}  // namespace eveng1
