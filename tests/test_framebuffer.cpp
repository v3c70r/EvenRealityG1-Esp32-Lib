#include <catch2/catch_test_macros.hpp>
#include "eveng1/eveng1.hpp"

using namespace eveng1;

TEST_CASE("Framebuffer default construction", "[framebuffer]") {
    Framebuffer fb;
    REQUIRE(fb.width == 576);
    REQUIRE(fb.height == 136);
    REQUIRE(fb.data.size() == 72 * 136);
}

TEST_CASE("Framebuffer custom construction", "[framebuffer]") {
    Framebuffer fb(64, 32);
    REQUIRE(fb.width == 64);
    REQUIRE(fb.height == 32);
    REQUIRE(fb.data.size() == 8 * 32);
}

TEST_CASE("Framebuffer clear", "[framebuffer]") {
    Framebuffer fb;
    fb.clear(false);
    for (auto b : fb.data) REQUIRE(b == 0x00);

    fb.clear(true);
    for (auto b : fb.data) REQUIRE(b == 0xFF);
}

TEST_CASE("Framebuffer setPixel/getPixel", "[framebuffer]") {
    Framebuffer fb;
    fb.clear(false);

    fb.setPixel(0, 0, true);
    REQUIRE(fb.getPixel(0, 0) == true);
    REQUIRE(fb.getPixel(1, 0) == false);

    fb.setPixel(7, 0, true);
    REQUIRE(fb.getPixel(7, 0) == true);

    fb.setPixel(8, 0, true);
    REQUIRE(fb.getPixel(8, 0) == true);
    REQUIRE(fb.getPixel(0, 0) == true);

    fb.setPixel(0, 0, false);
    REQUIRE(fb.getPixel(0, 0) == false);
}

TEST_CASE("Framebuffer setPixel out of bounds", "[framebuffer]") {
    Framebuffer fb;
    fb.clear(false);
    fb.setPixel(576, 0, true);
    fb.setPixel(0, 136, true);
    for (auto b : fb.data) REQUIRE(b == 0x00);
}

TEST_CASE("Framebuffer fillRect", "[framebuffer]") {
    Framebuffer fb;
    fb.clear(false);
    fb.fillRect(0, 0, 16, 8, true);

    for (uint16_t y = 0; y < 8; y++) {
        for (uint16_t x = 0; x < 16; x++) {
            REQUIRE(fb.getPixel(x, y) == true);
        }
    }
    REQUIRE(fb.getPixel(16, 0) == false);
    REQUIRE(fb.getPixel(0, 8) == false);
}

TEST_CASE("Framebuffer fillRect partial byte", "[framebuffer]") {
    Framebuffer fb;
    fb.clear(false);
    fb.fillRect(3, 0, 6, 1, true);

    for (uint16_t x = 3; x < 9; x++) {
        REQUIRE(fb.getPixel(x, 0) == true);
    }
    REQUIRE(fb.getPixel(2, 0) == false);
    REQUIRE(fb.getPixel(9, 0) == false);
}

TEST_CASE("Framebuffer drawLine", "[framebuffer]") {
    Framebuffer fb;
    fb.clear(false);
    fb.drawLine(0, 0, 10, 0);

    for (uint16_t x = 0; x <= 10; x++) {
        REQUIRE(fb.getPixel(x, 0) == true);
    }
}

TEST_CASE("Framebuffer drawLine diagonal", "[framebuffer]") {
    Framebuffer fb;
    fb.clear(false);
    fb.drawLine(0, 0, 4, 4);

    REQUIRE(fb.getPixel(0, 0) == true);
    REQUIRE(fb.getPixel(4, 4) == true);
}
