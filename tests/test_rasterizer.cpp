#include <catch2/catch_test_macros.hpp>
#include "eveng1/eveng1.hpp"
#include "eveng1/graphics/rasterizer.hpp"

using namespace eveng1;

TEST_CASE("Rasterizer grayscale to 1-bit", "[rasterizer]") {
    Framebuffer fb(16, 8);
    uint8_t gray[128];
    for (int i = 0; i < 128; i++) gray[i] = 255;
    eveng1::applyDither(fb, gray, 16, 8, Dither::NONE);

    for (int y = 0; y < 8; y++)
        for (int x = 0; x < 16; x++)
            REQUIRE(fb.getPixel(x, y) == true);
}

TEST_CASE("Rasterizer all black", "[rasterizer]") {
    Framebuffer fb(16, 8);
    uint8_t gray[128] = {0};
    eveng1::applyDither(fb, gray, 16, 8, Dither::NONE);

    for (int y = 0; y < 8; y++)
        for (int x = 0; x < 16; x++)
            REQUIRE(fb.getPixel(x, y) == false);
}

TEST_CASE("Rasterizer gradient with dithering", "[rasterizer]") {
    Framebuffer fb(32, 1);
    uint8_t gray[32];
    for (int i = 0; i < 32; i++) gray[i] = static_cast<uint8_t>(i * 8);
    eveng1::applyDither(fb, gray, 32, 1, Dither::ORDERED_2X2);

    // Left side should be mostly off, right side mostly on
    int leftOn = 0, rightOn = 0;
    for (int x = 0; x < 8; x++) if (fb.getPixel(x, 0)) leftOn++;
    for (int x = 24; x < 32; x++) if (fb.getPixel(x, 0)) rightOn++;
    REQUIRE(rightOn >= leftOn);
}
