#include "eveng1/protocol/g1_commands.hpp"
#include <cstdint>
#include <cstring>

namespace eveng1 {

struct PacketBuilder {
    uint8_t buffer[256];
    uint16_t len = 0;

    void reset() { len = 0; }

    void addByte(uint8_t b) { buffer[len++] = b; }

    void addBytes(const uint8_t* data, uint16_t dataLen) {
        std::memcpy(buffer + len, data, dataLen);
        len += dataLen;
    }

    void addCmdHeader(uint8_t cmd, uint16_t payloadLen) {
        addByte(cmd);
        addByte(static_cast<uint8_t>(payloadLen & 0xFF));
        addByte(static_cast<uint8_t>((payloadLen >> 8) & 0xFF));
    }
};

PacketBuilder buildInitPacket() {
    PacketBuilder pb;
    pb.addCmdHeader(CMD_INIT, 0);
    return pb;
}

PacketBuilder buildHeartbeatPacket() {
    PacketBuilder pb;
    pb.addBytes(HEARTBEAT_TEMPLATE, sizeof(HEARTBEAT_TEMPLATE));
    return pb;
}

PacketBuilder buildBmpDataPacket(const uint8_t addr[4], const uint8_t* data, uint16_t dataLen) {
    PacketBuilder pb;
    uint16_t payloadLen = 4 + dataLen;
    pb.addCmdHeader(CMD_BMP_DATA, payloadLen);
    pb.addBytes(addr, 4);
    pb.addBytes(data, dataLen);
    return pb;
}

PacketBuilder buildBmpEndPacket() {
    PacketBuilder pb;
    pb.addCmdHeader(CMD_BMP_END, 0);
    return pb;
}

PacketBuilder buildCrcPacket(const uint8_t addr[4], const uint8_t crc[4]) {
    PacketBuilder pb;
    uint16_t payloadLen = 4 + 4;
    pb.addCmdHeader(CMD_CRC, payloadLen);
    pb.addBytes(addr, 4);
    pb.addBytes(crc, 4);
    return pb;
}

PacketBuilder buildExitPacket() {
    PacketBuilder pb;
    pb.addCmdHeader(CMD_EXIT, 0);
    return pb;
}

PacketBuilder buildTextPacket(const char* text, uint8_t page, uint8_t maxPage) {
    PacketBuilder pb;
    uint16_t textLen = static_cast<uint16_t>(std::strlen(text));
    uint16_t payloadLen = 2 + textLen;  // page + maxPage + text
    pb.addCmdHeader(CMD_TEXT, payloadLen);
    pb.addByte(page);
    pb.addByte(maxPage);
    pb.addBytes(reinterpret_cast<const uint8_t*>(text), textLen);
    return pb;
}

}  // namespace eveng1
