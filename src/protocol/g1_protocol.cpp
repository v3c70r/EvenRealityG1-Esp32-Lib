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
    // Send INIT command (0xF4, 0x01) - matches reference app
    uint8_t initPacket[] = {CMD_INIT, 0x01};
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
    uint8_t exitPacket[] = {CMD_EXIT, 0x01};
    sendPacket(exitPacket, sizeof(exitPacket));
}

bool G1Protocol::sendBmp(const uint8_t* pixelData, uint32_t dataLen,
                          const uint8_t addr[4],
                          uint32_t interPacketDelayMs) {
    if (!pixelData || dataLen == 0) return false;

    // Calculate CRC-32/XZ over address + pixel data
    uint8_t crc[4];
    crc32_xz_bmp(addr, pixelData, dataLen, crc);

    // Send CRC command FIRST (before BMP data)
    // Format: [0x16, crc0, crc1, crc2, crc3] - 5 bytes, NO address
    uint8_t crcPacket[5];
    crcPacket[0] = CMD_CRC;
    std::memcpy(crcPacket + 1, crc, 4);
    if (!sendPacket(crcPacket, sizeof(crcPacket))) {
        std::cerr << "Failed to send CRC" << std::endl;
        return false;
    }

    // Send BMP data in chunks
    const uint32_t maxPayload = BMP_PACKET_SIZE;  // 194 bytes of pixel data per packet
    uint32_t offset = 0;
    uint8_t packetBuffer[BMP_PACKET_SIZE + 10];

    while (offset < dataLen) {
        uint32_t chunkLen = std::min(maxPayload, dataLen - offset);

        // Build BMP_DATA packet
        // First packet: [0x15, seq, addr[4], data...]
        // Subsequent: [0x15, seq, data...]
        bool isFirst = (offset == 0);
        size_t headerSize = isFirst ? 6 : 2;
        
        packetBuffer[0] = CMD_BMP_DATA;
        packetBuffer[1] = (offset / BMP_PACKET_SIZE) & 0xFF;  // seq
        if (isFirst) {
            std::memcpy(packetBuffer + 2, addr, 4);
        }
        std::memcpy(packetBuffer + headerSize, pixelData + offset, chunkLen);

        uint16_t totalLen = static_cast<uint16_t>(headerSize + chunkLen);
        if (!sendPacket(packetBuffer, totalLen)) {
            std::cerr << "Failed to send BMP data at offset " << offset << std::endl;
            return false;
        }

        offset += chunkLen;

        if (interPacketDelayMs > 0) {
            std::this_thread::sleep_for(std::chrono::milliseconds(interPacketDelayMs));
        }
    }

    // Send BMP_END: [0x20, 0x0D, 0x0E]
    uint8_t endPacket[] = {CMD_BMP_END, 0x0D, 0x0E};
    if (!sendPacket(endPacket, sizeof(endPacket))) {
        std::cerr << "Failed to send BMP_END" << std::endl;
        return false;
    }

    return true;
}

bool G1Protocol::sendText(const char* text, uint8_t page, uint8_t maxPage) {
    if (!text) return false;

    uint16_t textLen = static_cast<uint16_t>(std::strlen(text));
    constexpr uint16_t MAX_CHUNK = 191;
    uint16_t totalPackets = (textLen + MAX_CHUNK - 1) / MAX_CHUNK;

    for (uint16_t i = 0; i < totalPackets; i++) {
        uint16_t start = i * MAX_CHUNK;
        uint16_t chunkLen = std::min(MAX_CHUNK, static_cast<uint16_t>(textLen - start));

        // 9-byte header: [0x4E, seq, totalPackets, seq, screenStatus, charPosHi, charPosLo, page, maxPage]
        std::vector<uint8_t> packet(9 + chunkLen);
        packet[0] = CMD_TEXT;
        packet[1] = i & 0xFF;           // seq
        packet[2] = totalPackets & 0xFF; // total packets
        packet[3] = i & 0xFF;           // seq (again)
        packet[4] = 0x71;               // screenStatus: 0x70 (text show) | 0x01 (new content)
        packet[5] = 0x00;               // char_pos high
        packet[6] = 0x00;               // char_pos low
        packet[7] = page;
        packet[8] = maxPage;
        std::memcpy(packet.data() + 9, text + start, chunkLen);

        if (!sendPacket(packet.data(), static_cast<uint16_t>(packet.size()))) {
            return false;
        }
    }

    return true;
}

bool G1Protocol::heartbeat() {
    m_hbSeq++;
    uint8_t hb[] = {CMD_HEARTBEAT, 0x06, 0x00, m_hbSeq, 0x04, m_hbSeq};
    return sendPacket(hb, sizeof(hb));
}

bool G1Protocol::sendBrightness(uint8_t level, bool autoLight) {
    if (level > BRIGHTNESS_MAX) level = BRIGHTNESS_MAX;
    uint8_t packet[] = {CMD_BRIGHTNESS, level, static_cast<uint8_t>(autoLight ? 1 : 0)};
    return sendPacket(packet, sizeof(packet));
}

bool G1Protocol::sendHeadUpAngle(uint8_t angle) {
    if (angle > HEAD_UP_ANGLE_MAX) angle = HEAD_UP_ANGLE_MAX;
    uint8_t packet[] = {CMD_HEAD_UP_ANGLE, angle, 0x01};
    return sendPacket(packet, sizeof(packet));
}

bool G1Protocol::sendDashboardPosition(uint8_t height, uint8_t depth) {
    if (height > DASHBOARD_HEIGHT_MAX) height = DASHBOARD_HEIGHT_MAX;
    if (depth < DASHBOARD_DEPTH_MIN) depth = DASHBOARD_DEPTH_MIN;
    if (depth > DASHBOARD_DEPTH_MAX) depth = DASHBOARD_DEPTH_MAX;
    static uint8_t counter = 0;
    uint8_t packet[] = {CMD_DASHBOARD_POS, 0x08, 0x00, counter++, 0x02, 0x01, height, depth};
    return sendPacket(packet, sizeof(packet));
}

bool G1Protocol::queryBattery() {
    uint8_t packet[] = {CMD_BATTERY_QUERY, 0x01};
    return sendPacket(packet, sizeof(packet));
}

bool G1Protocol::setWearDetection(bool enabled) {
    uint8_t packet[] = {CMD_WEAR_DETECTION, static_cast<uint8_t>(enabled ? 0x01 : 0x00)};
    return sendPacket(packet, sizeof(packet));
}

bool G1Protocol::setSilentMode(bool enabled) {
    uint8_t packet[] = {CMD_SILENT_MODE, static_cast<uint8_t>(enabled ? 0x01 : 0x0A)};
    return sendPacket(packet, sizeof(packet));
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
        // Process BLE events via transport's blocking wait
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
