#pragma once
#include <cstdint>
#include <cstddef>
#include "g1_commands.hpp"

namespace eveng1 {

struct BmpRegion {
    uint32_t x, y;          // offset in pixels
    uint32_t width, height; // region size

    uint32_t rowBytes() const { return width / 8; }
    uint32_t startAddressByte() const {
        // Row-major framebuffer: each row = 72 bytes
        return BMP_BASE_ADDRESS + y * DISPLAY_ROW_BYTES + (x / 8);
    }
    uint32_t dataSize() const { return height * rowBytes(); }
};

struct TestCase {
    const char* id;
    const char* name;
    BmpRegion region;
    bool useBaseAddress; // If true, use BMP_BASE_ADDRESS for CRC
};

// Full framebuffer = DISPLAY_HEIGHT * DISPLAY_ROW_BYTES = 9792 bytes
constexpr size_t FULL_FB_SIZE = DISPLAY_HEIGHT * DISPLAY_ROW_BYTES;

// ─── Test Matrix ───

extern const TestCase BMP_TESTS[];
extern const int BMP_TEST_COUNT;

// ─── Pattern Generators ───

/// Generate a checkerboard pattern
void generateCheckerboard(uint8_t* fb);

/// Generate horizontal stripes (4-row bands)
void generateStripes(uint8_t* fb);

/// Generate test-specific pattern
void generateTestPattern(const char* testId, uint8_t* fb);

/// Extract region data from full framebuffer
void extractRegion(const uint8_t* fb, const BmpRegion& region, uint8_t* out);

}  // namespace eveng1
