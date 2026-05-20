#include "eveng1/protocol/g1_protocol.hpp"
#include "eveng1/graphics/crc32_xz.hpp"
#include <cstring>
#include <thread>
#include <chrono>
#include <iostream>

namespace eveng1 {

G1Protocol::G1Protocol(BleTransport& transport)
    : m_transport(transport) {
    m_transport.setNotifyCallback(
        [this](uint8_t cmd, const uint8_t* data, uint16_t len) {
            m_lastResponseCmd = cmd;
            m_lastResponseLen = len;
            if (len <= sizeof(m_lastResponseData)) {
                std::memcpy(m_lastResponseData, data, len);
            }
            m_responseReady = true;

            if (m_responseCb) {
                m_responseCb(cmd, data, len);
            }
        });
}

bool G1Protocol::init() {
    // Send INIT command (0xF4)
    uint8_t initPacket[] = {CMD_INIT, 0x00, 0x00};
    if (!sendPacket(initPacket, sizeof(initPacket))) {
        std::cerr << "Failed to send INIT" << std::endl;
        return false;
    }

    // Wait for success response
    if (!waitForResponse(RSP_SUCCESS, 5000)) {
        std::cerr << "INIT failed: no response" << std::endl;
        return false;
    }

    return true;
}

void G1Protocol::exit() {
    uint8_t exitPacket[] = {CMD_EXIT, 0x00, 0x00};
    sendPacket(exitPacket, sizeof(exitPacket));
}

bool G1Protocol::sendBmp(const uint8_t* pixelData, uint32_t dataLen,
                          const uint8_t addr[4],
                          uint32_t interPacketDelayMs) {
    if (!pixelData || dataLen == 0) return false;

    // Calculate CRC-32/XZ over address + pixel data
    uint8_t crc[4];
    crc32_xz_bmp(addr, pixelData, dataLen, crc);

    // Send CRC command first
    uint8_t crcPacket[11];
    crcPacket[0] = CMD_CRC;
    crcPacket[1] = 0x08;  // payload length (4 addr + 4 crc)
    crcPacket[2] = 0x00;
    std::memcpy(crcPacket + 3, addr, 4);
    std::memcpy(crcPacket + 7, crc, 4);
    if (!sendPacket(crcPacket, sizeof(crcPacket))) {
        std::cerr << "Failed to send CRC" << std::endl;
        return false;
    }

    // Send BMP data in chunks
    const uint32_t maxPayload = BMP_PACKET_SIZE - 4;  // 190 bytes of pixel data per packet
    uint32_t offset = 0;
    uint8_t packetBuffer[BMP_PACKET_SIZE + 10];

    while (offset < dataLen) {
        uint32_t chunkLen = std::min(maxPayload, dataLen - offset);

        // Build BMP_DATA packet
        packetBuffer[0] = CMD_BMP_DATA;
        packetBuffer[1] = static_cast<uint8_t>((4 + chunkLen) & 0xFF);
        packetBuffer[2] = static_cast<uint8_t>(((4 + chunkLen) >> 8) & 0xFF);
        std::memcpy(packetBuffer + 3, addr, 4);
        std::memcpy(packetBuffer + 7, pixelData + offset, chunkLen);

        uint16_t totalLen = 3 + 4 + chunkLen;
        if (!sendPacket(packetBuffer, totalLen)) {
            std::cerr << "Failed to send BMP data at offset " << offset << std::endl;
            return false;
        }

        offset += chunkLen;

        // Update address for next packet (big-endian 32-bit)
        uint32_t addrVal = (static_cast<uint32_t>(addr[0]) << 24) |
                           (static_cast<uint32_t>(addr[1]) << 16) |
                           (static_cast<uint32_t>(addr[2]) << 8) |
                            static_cast<uint32_t>(addr[3]);
        addrVal += chunkLen;
        const_cast<uint8_t*>(addr)[0] = (addrVal >> 24) & 0xFF;
        const_cast<uint8_t*>(addr)[1] = (addrVal >> 16) & 0xFF;
        const_cast<uint8_t*>(addr)[2] = (addrVal >> 8) & 0xFF;
        const_cast<uint8_t*>(addr)[3] = addrVal & 0xFF;

        if (interPacketDelayMs > 0) {
            std::this_thread::sleep_for(std::chrono::milliseconds(interPacketDelayMs));
        }
    }

    // Send BMP_END
    uint8_t endPacket[] = {CMD_BMP_END, 0x00, 0x00};
    if (!sendPacket(endPacket, sizeof(endPacket))) {
        std::cerr << "Failed to send BMP_END" << std::endl;
        return false;
    }

    return true;
}

bool G1Protocol::sendText(const char* text, uint8_t page, uint8_t maxPage) {
    if (!text) return false;

    uint16_t textLen = static_cast<uint16_t>(std::strlen(text));
    uint16_t payloadLen = 2 + textLen;

    std::vector<uint8_t> packet(3 + payloadLen);
    packet[0] = CMD_TEXT;
    packet[1] = payloadLen & 0xFF;
    packet[2] = (payloadLen >> 8) & 0xFF;
    packet[3] = page;
    packet[4] = maxPage;
    std::memcpy(packet.data() + 5, text, textLen);

    return sendPacket(packet.data(), static_cast<uint16_t>(packet.size()));
}

bool G1Protocol::heartbeat() {
    return sendPacket(HEARTBEAT_TEMPLATE, sizeof(HEARTBEAT_TEMPLATE));
}

void G1Protocol::setResponseCallback(ResponseCallback cb) {
    m_responseCb = cb;
}

bool G1Protocol::sendPacket(const uint8_t* data, uint16_t len) {
    m_responseReady = false;
    return m_transport.write(data, len);
}

bool G1Protocol::waitForResponse(uint8_t expectedCmd, uint32_t timeoutMs) {
    auto start = std::chrono::steady_clock::now();
    while (std::chrono::duration_cast<std::chrono::milliseconds>(
               std::chrono::steady_clock::now() - start).count() < static_cast<int64_t>(timeoutMs)) {
        // Process D-Bus events
        if (m_transport.isConnected()) {
            m_transport.waitForNotify(m_lastResponseLen, 100);
        }

        if (m_responseReady) {
            return m_lastResponseCmd == expectedCmd;
        }
    }
    return false;
}

}  // namespace eveng1
