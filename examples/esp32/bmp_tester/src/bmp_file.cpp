#include "bmp_file.hpp"
#include <cstring>

namespace eveng1 {

void generateBmpFile(uint8_t* bmp, bool checkerboard) {
    memset(bmp, 0, BMP_FULL_SIZE);

    // BMP File Header (14 bytes)
    BmpFileHeader* fileHdr = reinterpret_cast<BmpFileHeader*>(bmp);
    fileHdr->signature = 0x4D42; // 'BM'
    fileHdr->fileSize = BMP_FULL_SIZE;
    fileHdr->reserved1 = 0;
    fileHdr->reserved2 = 0;
    fileHdr->dataOffset = BMP_HEADER_SIZE;

    // BMP Info Header (40 bytes)
    BmpInfoHeader* infoHdr = reinterpret_cast<BmpInfoHeader*>(bmp + 14);
    infoHdr->headerSize = 40;
    infoHdr->width = 576;
    infoHdr->height = 136; // Positive = bottom-up
    infoHdr->planes = 1;
    infoHdr->bitCount = 1;
    infoHdr->compression = 0;
    infoHdr->imageSize = BMP_PIXEL_SIZE;
    infoHdr->xPixelsPerMeter = 2834;
    infoHdr->yPixelsPerMeter = 2834;
    infoHdr->colorsUsed = 0;
    infoHdr->colorsImportant = 0;

    // Color table (8 bytes): 2 entries for 1-bit
    // Entry 0: Black (0,0,0,0)
    // Entry 1: White (255,255,255,0)
    bmp[54] = 0; bmp[55] = 0; bmp[56] = 0; bmp[57] = 0;       // Black
    bmp[58] = 255; bmp[59] = 255; bmp[60] = 255; bmp[61] = 0; // White

    // Pixel data starts at offset 62
    // BMP stores rows bottom-to-top
    uint8_t* pixelData = bmp + BMP_HEADER_SIZE;

    for (int bmpRow = 0; bmpRow < 136; bmpRow++) {
        // BMP stores bottom row first, so we need to invert
        int fbRow = 135 - bmpRow;
        uint8_t* rowPtr = pixelData + bmpRow * BMP_ROW_BYTES;

        for (int x = 0; x < 576; x++) {
            bool setPixel = false;
            if (checkerboard) {
                int cx = x / 8;
                int cy = fbRow / 8;
                setPixel = ((cx + cy) % 2 == 0);
            } else {
                // Simple pattern: white rectangle in center
                setPixel = (fbRow >= 20 && fbRow < 116 && x >= 50 && x < 526);
            }

            if (setPixel) {
                int byteIdx = x / 8;
                int bitIdx = 7 - (x % 8); // BMP stores MSB first
                rowPtr[byteIdx] |= (1 << bitIdx);
            }
        }
    }
}

}  // namespace eveng1
