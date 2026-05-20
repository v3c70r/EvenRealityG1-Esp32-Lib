#pragma once

#include "../eveng1.hpp"
#include "../protocol/g1_commands.hpp"
#include "../devices/device.hpp"
#include <cstdint>
#include <cstddef>
#include <functional>
#include <memory>
#include <chrono>

namespace eveng1 {

class G1Protocol {
public:
    explicit G1Protocol(BleTransport& transport);

    // Connection lifecycle
    bool init();
    void exit();

    // BMP transfer
    bool sendBmp(const uint8_t* pixelData, uint32_t dataLen,
                 const uint8_t addr[4],
                 uint32_t interPacketDelayMs = 15);

    // Text display
    bool sendText(const char* text, uint8_t page = 1, uint8_t maxPage = 1);

    // Heartbeat
    bool heartbeat();

    // Response handling
    using ResponseCallback = std::function<void(uint8_t cmd, const uint8_t* data, uint16_t len)>;
    void setResponseCallback(ResponseCallback cb);

private:
    bool sendPacket(const uint8_t* data, uint16_t len);
    bool waitForResponse(uint8_t expectedCmd, uint32_t timeoutMs = 3000);

    BleTransport& m_transport;
    ResponseCallback m_responseCb;
    uint8_t m_lastResponseCmd = 0;
    uint8_t m_lastResponseData[256];
    uint16_t m_lastResponseLen = 0;
    bool m_responseReady = false;
};

}  // namespace eveng1
