#include <catch2/catch_test_macros.hpp>
#include "eveng1/eveng1.hpp"
#include "eveng1/graphics/rasterizer.hpp"

using namespace eveng1;

TEST_CASE("Dither NONE threshold", "[dither]") {
    Framebuffer fb(8, 1);
    uint8_t gray[8] = {0, 64, 127, 128, 129, 200, 255, 100};
    eveng1::applyDither(fb, gray, 8, 1, Dither::NONE);

    REQUIRE(fb.getPixel(0, 0) == false);   // 0
    REQUIRE(fb.getPixel(1, 0) == false);   // 64
    REQUIRE(fb.getPixel(2, 0) == false);   // 127
    REQUIRE(fb.getPixel(3, 0) == true);    // 128
    REQUIRE(fb.getPixel(4, 0) == true);    // 129
    REQUIRE(fb.getPixel(5, 0) == true);    // 200
    REQUIRE(fb.getPixel(6, 0) == true);    // 255
}

TEST_CASE("Dither ORDERED_2X2 produces variation", "[dither]") {
    Framebuffer fb(4, 2);
    uint8_t gray[8] = {100, 100, 100, 100, 100, 100, 100, 100};
    eveng1::applyDither(fb, gray, 4, 2, Dither::ORDERED_2X2);

    // With uniform mid-gray and 2x2 dither, not all pixels should be the same
    int onCount = 0;
    for (int y = 0; y < 2; y++)
        for (int x = 0; x < 4; x++)
            if (fb.getPixel(x, y)) onCount++;
    REQUIRE(onCount > 0);
    REQUIRE(onCount < 8);
}

TEST_CASE("Dither ORDERED_4X4 produces variation", "[dither]") {
    Framebuffer fb(8, 4);
    uint8_t gray[32];
    for (int i = 0; i < 32; i++) gray[i] = 128;
    eveng1::applyDither(fb, gray, 8, 4, Dither::ORDERED_4X4);

    int onCount = 0;
    for (int y = 0; y < 4; y++)
        for (int x = 0; x < 8; x++)
            if (fb.getPixel(x, y)) onCount++;
    REQUIRE(onCount > 0);
    REQUIRE(onCount < 32);
}

TEST_CASE("Dither FLOYD_STEINBERG produces variation", "[dither]") {
    Framebuffer fb(16, 8);
    uint8_t gray[128];
    for (int i = 0; i < 128; i++) gray[i] = 100;
    eveng1::applyDither(fb, gray, 16, 8, Dither::FLOYD_STEINBERG);

    int onCount = 0;
    for (int y = 0; y < 8; y++)
        for (int x = 0; x < 16; x++)
            if (fb.getPixel(x, y)) onCount++;
    REQUIRE(onCount > 0);
    REQUIRE(onCount < 128);
}
