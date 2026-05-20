#pragma once

#include <cstdint>
#include <cstddef>
#include <vector>
#include <functional>
#include <string>

namespace eveng1 {

// ─── Core Types ───

struct Vec2 {
    uint16_t x = 0;
    uint16_t y = 0;
};

struct Rect {
    uint16_t x = 0;
    uint16_t y = 0;
    uint16_t width = 0;
    uint16_t height = 0;

    bool isEmpty() const { return width == 0 || height == 0; }

    Rect unionWith(const Rect& other) const;
    bool contains(uint16_t px, uint16_t py) const;
};

struct DirtyRect : Rect {
    void expand(uint16_t px, uint16_t py);
    void expand(const Rect& r);
    void reset();
};

// ─── Render Style ───

enum class Dither : uint8_t {
    NONE = 0,
    ORDERED_2X2,   // 4-level grayscale
    ORDERED_4X4,   // 16-level grayscale
    FLOYD_STEINBERG,
};

struct RenderStyle {
    Dither dither = Dither::NONE;
    bool invert = false;
};

// ─── Pixel Formats ───

/// 1-bit packed framebuffer: row-major, MSB = leftmost pixel
/// 576 × 136 = 9792 bytes = 72 bytes per row
struct Framebuffer {
    static constexpr uint32_t DEFAULT_WIDTH = 576;
    static constexpr uint32_t DEFAULT_HEIGHT = 136;
    static constexpr uint32_t ROW_BYTES = DEFAULT_WIDTH / 8;  // 72

    uint32_t width = DEFAULT_WIDTH;
    uint32_t height = DEFAULT_HEIGHT;
    std::vector<uint8_t> data;  // height * (width/8) bytes

    Framebuffer();
    Framebuffer(uint32_t w, uint32_t h);

    void clear(bool value = false);

    void setPixel(uint16_t x, uint16_t y, bool on);
    bool getPixel(uint16_t x, uint16_t y) const;

    void fillRect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, bool value);
    void drawLine(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1);

    /// Blit a glyph from bitmap font data at (x, y)
    void blitGlyph(uint16_t x, uint16_t y,
                   const uint8_t* glyphData, uint8_t glyphW, uint8_t glyphH,
                   RenderStyle style = {});

    size_t byteSize() const { return data.size(); }
};

// ─── Font ───

struct BitmapFont {
    const uint8_t* data;      // glyph bitmap table
    uint8_t glyphWidth;       // width of each glyph in pixels
    uint8_t glyphHeight;      // height of each glyph in pixels
    uint8_t firstChar;        // first ASCII char in table (usually 32 = space)
    uint8_t charCount;        // number of glyphs in table

    /// Get pointer to glyph data for character c.
    /// Returns nullptr if c is not in range.
    const uint8_t* glyph(char c) const;

    /// Measure width of a text string in pixels.
    uint16_t measureWidth(const char* text) const;

    static const BitmapFont& defaultFont();  // 5×7 monospace
};

// ─── Input Events ───

enum class TouchEvent : uint8_t {
    SINGLE_TAP_LEFT,
    SINGLE_TAP_RIGHT,
    DOUBLE_TAP_LEFT,
    DOUBLE_TAP_RIGHT,
    TRIPLE_TAP_LEFT,
    TRIPLE_TAP_RIGHT,
    LONG_PRESS_LEFT,
    LONG_PRESS_RIGHT,
    LONG_PRESS_RELEASE,
    WEARING,
    NOT_WEARING,
    UNKNOWN,
};

using InputCallback = std::function<void(TouchEvent event, bool isLeft)>;

}  // namespace eveng1
