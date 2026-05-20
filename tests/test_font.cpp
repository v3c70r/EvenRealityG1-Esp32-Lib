#include <catch2/catch_test_macros.hpp>
#include "eveng1/eveng1.hpp"

using namespace eveng1;

TEST_CASE("Default font exists", "[font]") {
    const auto& font = BitmapFont::defaultFont();
    REQUIRE(font.glyphWidth == 5);
    REQUIRE(font.glyphHeight == 7);
    REQUIRE(font.firstChar == 0x20);
    REQUIRE(font.charCount == 95);
}

TEST_CASE("Font glyph retrieval", "[font]") {
    const auto& font = BitmapFont::defaultFont();

    REQUIRE(font.glyph(' ') != nullptr);
    REQUIRE(font.glyph('A') != nullptr);
    REQUIRE(font.glyph('z') != nullptr);
    REQUIRE(font.glyph('~') != nullptr);
    REQUIRE(font.glyph(0x1F) == nullptr);
    REQUIRE(font.glyph(0x7F) == nullptr);
}

TEST_CASE("Font glyph size", "[font]") {
    const auto& font = BitmapFont::defaultFont();
    const uint8_t* g = font.glyph('A');
    REQUIRE(g != nullptr);
    // Each glyph is glyphHeight bytes (7 bytes for 5x7)
    // Just verify it's accessible
    (void)g[0];
    (void)g[6];
}

TEST_CASE("Font measure width", "[font]") {
    const auto& font = BitmapFont::defaultFont();

    REQUIRE(font.measureWidth("") == 0);
    REQUIRE(font.measureWidth("A") == 5);
    REQUIRE(font.measureWidth("AB") == 10);
    REQUIRE(font.measureWidth("Hello") == 25);
}

TEST_CASE("Font blit glyph", "[font]") {
    Framebuffer fb(64, 16);
    fb.clear(false);

    const auto& font = BitmapFont::defaultFont();
    const uint8_t* g = font.glyph('X');
    REQUIRE(g != nullptr);

    fb.blitGlyph(0, 0, g, font.glyphWidth, font.glyphHeight);

    // Verify some pixels were drawn
    bool hasPixels = false;
    for (uint16_t y = 0; y < 7; y++) {
        for (uint16_t x = 0; x < 5; x++) {
            if (fb.getPixel(x, y)) hasPixels = true;
        }
    }
    REQUIRE(hasPixels);
}
