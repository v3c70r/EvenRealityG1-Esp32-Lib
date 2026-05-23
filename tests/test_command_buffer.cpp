#include <catch2/catch_test_macros.hpp>
#include "eveng1/protocol/g1_commands.hpp"

using namespace eveng1;

TEST_CASE("Command constants", "[command_buffer]") {
    REQUIRE(CMD_INIT == 0xF4);
    REQUIRE(CMD_HEARTBEAT == 0x25);
    REQUIRE(CMD_BMP_DATA == 0x15);
    REQUIRE(CMD_CRC == 0x16);
    REQUIRE(CMD_EXIT == 0x18);
    REQUIRE(CMD_BMP_END == 0x20);
    REQUIRE(CMD_TEXT == 0x4E);
    REQUIRE(RSP_SUCCESS == 0xC9);
    REQUIRE(RSP_FAILURE == 0xCA);
}

TEST_CASE("BMP packet size constant", "[command_buffer]") {
    REQUIRE(BMP_PACKET_SIZE == 194);
}

TEST_CASE("Display dimensions", "[command_buffer]") {
    REQUIRE(DISPLAY_WIDTH == 576);
    REQUIRE(DISPLAY_HEIGHT == 136);
    REQUIRE(DISPLAY_ROW_BYTES == 72);
}
