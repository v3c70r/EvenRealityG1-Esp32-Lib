#include <catch2/catch_test_macros.hpp>
#include "eveng1/protocol/g1_commands.hpp"

using namespace eveng1;

// PacketBuilder functions from display_sender.cpp
struct PacketBuilder {
    uint8_t buffer[256];
    uint16_t len = 0;
    void reset();
    void addByte(uint8_t b);
    void addBytes(const uint8_t* data, uint16_t dataLen);
    void addCmdHeader(uint8_t cmd, uint16_t payloadLen);
};

namespace eveng1 {
    PacketBuilder buildInitPacket();
    PacketBuilder buildHeartbeatPacket();
    PacketBuilder buildBmpEndPacket();
    PacketBuilder buildExitPacket();
    PacketBuilder buildTextPacket(const char* text, uint8_t page, uint8_t maxPage);
}

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

TEST_CASE("Build init packet", "[command_buffer]") {
    auto pb = eveng1::buildInitPacket();
    REQUIRE(pb.len >= 1);
    REQUIRE(pb.buffer[0] == CMD_INIT);
}

TEST_CASE("Build heartbeat packet", "[command_buffer]") {
    auto pb = eveng1::buildHeartbeatPacket();
    REQUIRE(pb.len >= 1);
    REQUIRE(pb.buffer[0] == 0x25);
}

TEST_CASE("Build BMP end packet", "[command_buffer]") {
    auto pb = eveng1::buildBmpEndPacket();
    REQUIRE(pb.len >= 1);
    REQUIRE(pb.buffer[0] == CMD_BMP_END);
}

TEST_CASE("Build exit packet", "[command_buffer]") {
    auto pb = eveng1::buildExitPacket();
    REQUIRE(pb.len >= 1);
    REQUIRE(pb.buffer[0] == CMD_EXIT);
}

TEST_CASE("Build text packet", "[command_buffer]") {
    auto pb = eveng1::buildTextPacket("Hello", 1, 1);
    REQUIRE(pb.len >= 3);
    REQUIRE(pb.buffer[0] == CMD_TEXT);
}
