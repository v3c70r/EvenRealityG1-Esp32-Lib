#include "bmp_pattern.hpp"
#include "g1_commands.hpp"
#include <cstring>

namespace eveng1 {

// ─── Test Matrix ───

const TestCase BMP_TESTS[] = {
    {"T1", "Full frame baseline",
     {0, 0, 576, 136}},
    {"T2", "Half height from origin",
     {0, 0, 576, 68}},
    {"T3", "Offset + partial height (rows 20-69)",
     {0, 20, 576, 50}},
    {"T4", "Bottom half offset (rows 68-117)",
     {0, 68, 576, 50}},
    {"T5", "Small rect at origin",
     {0, 0, 100, 40}},
    {"T6", "Offset + reduced width",
     {10, 10, 200, 30}},
    {"T7", "Alternative address test",
     {0, 0, 576, 136}},
    {"T8", "Single-row update",
     {0, 50, 576, 1}},
    {"T9", "10-row strip (clock-digit sized)",
     {0, 40, 576, 10}},
};

const int BMP_TEST_COUNT = sizeof(BMP_TESTS) / sizeof(BMP_TESTS[0]);

// ─── Pattern Generators ───

void generateCheckerboard(uint8_t* fb) {
    memset(fb, 0, FULL_FB_SIZE);
    const int checker = 8;
    for (int y = 0; y < DISPLAY_HEIGHT; ++y) {
        for (int x = 0; x < DISPLAY_WIDTH; ++x) {
            if (((x / checker) + (y / checker)) % 2 == 0) {
                size_t byteIdx = y * DISPLAY_ROW_BYTES + x / 8;
                int bitIdx = 7 - (x % 8);
                fb[byteIdx] |= (1 << bitIdx);
            }
        }
    }
}

void generateStripes(uint8_t* fb) {
    memset(fb, 0, FULL_FB_SIZE);
    for (int y = 0; y < DISPLAY_HEIGHT; ++y) {
        if ((y / 4) % 2 == 0) {
            for (int x = 0; x < DISPLAY_ROW_BYTES; ++x) {
                fb[y * DISPLAY_ROW_BYTES + x] = 0xFF;
            }
        }
    }
}

void generateTestPattern(const char* testId, uint8_t* fb) {
    memset(fb, 0, FULL_FB_SIZE);

    // Each test gets a visually distinct pattern
    if (strcmp(testId, "T1") == 0) {
        // Full checkerboard
        generateCheckerboard(fb);
    } else if (strcmp(testId, "T2") == 0) {
        // Top half checkerboard (rows 0-67)
        const int checker = 8;
        for (int y = 0; y < 68; ++y) {
            for (int x = 0; x < DISPLAY_WIDTH; ++x) {
                if (((x / checker) + (y / checker)) % 2 == 0) {
                    size_t byteIdx = y * DISPLAY_ROW_BYTES + x / 8;
                    int bitIdx = 7 - (x % 8);
                    fb[byteIdx] |= (1 << bitIdx);
                }
            }
        }
    } else if (strcmp(testId, "T3") == 0 || strcmp(testId, "T4") == 0 ||
               strcmp(testId, "T9") == 0) {
        // Solid white block for offset tests (rows 20-69, 68-117, 40-49)
        int yStart = 20, yEnd = 70;
        if (strcmp(testId, "T4") == 0) { yStart = 68; yEnd = 118; }
        if (strcmp(testId, "T9") == 0) { yStart = 40; yEnd = 50; }
        for (int y = yStart; y < yEnd; ++y) {
            for (int x = 0; x < DISPLAY_ROW_BYTES; ++x) {
                fb[y * DISPLAY_ROW_BYTES + x] = 0xFF;
            }
        }
    } else if (strcmp(testId, "T5") == 0) {
        // 100×40 white rect at origin
        for (int y = 0; y < 40; ++y) {
            for (int x = 0; x < 100; ++x) {
                size_t byteIdx = y * DISPLAY_ROW_BYTES + x / 8;
                int bitIdx = 7 - (x % 8);
                fb[byteIdx] |= (1 << bitIdx);
            }
        }
    } else if (strcmp(testId, "T6") == 0) {
        // 200×30 at offset (10,10)
        for (int y = 10; y < 40; ++y) {
            for (int x = 10; x < 210; ++x) {
                size_t byteIdx = y * DISPLAY_ROW_BYTES + x / 8;
                int bitIdx = 7 - (x % 8);
                fb[byteIdx] |= (1 << bitIdx);
            }
        }
    } else if (strcmp(testId, "T7") == 0) {
        // Full checkerboard (test for alternative address)
        generateCheckerboard(fb);
    } else if (strcmp(testId, "T8") == 0) {
        // Single solid row at y=50
        int y = 50;
        for (int x = 0; x < DISPLAY_ROW_BYTES; ++x) {
            fb[y * DISPLAY_ROW_BYTES + x] = 0xFF;
        }
    }
}

void extractRegion(const uint8_t* fb, const BmpRegion& region, uint8_t* out) {
    size_t outOff = 0;
    for (uint32_t row = region.y; row < region.y + region.height; ++row) {
        size_t start = row * DISPLAY_ROW_BYTES + region.x / 8;
        size_t rowBytes = region.width / 8;
        memcpy(out + outOff, fb + start, rowBytes);
        outOff += rowBytes;
    }
}

}  // namespace eveng1
