#include <catch2/catch_test_macros.hpp>
#include "eveng1/devices/device.hpp"
#include "eveng1/protocol/g1_protocol.hpp"
#include "eveng1/protocol/g1_commands.hpp"
#include <cstring>
#include <thread>
#include <atomic>

using namespace eveng1;

// NullTransport implementation for testing
class TestTransport : public BleTransport {
public:
    bool connect(const char* address) override {
        m_connected = true;
        return true;
    }
    void disconnect() override { m_connected = false; }
    bool isConnected() const override { return m_connected; }

    bool write(const uint8_t* data, uint16_t len) override {
        m_lastWrite.assign(data, data + len);
        m_writeCount++;

        // Auto-respond to INIT with success
        if (len >= 1 && data[0] == CMD_INIT && m_notifyCb) {
            uint8_t rsp[] = {RSP_SUCCESS};
            m_notifyCb(RSP_SUCCESS, rsp, sizeof(rsp));
        }

        return true;
    }

    void setNotifyCallback(NotifyCallback cb) override { m_notifyCb = cb; }

    uint8_t* waitForNotify(uint16_t& outLen, uint32_t timeoutMs) override {
        outLen = 0;
        return nullptr;
    }

    // Simulate incoming response
    void simulateResponse(uint8_t cmd, const uint8_t* data, uint16_t len) {
        if (m_notifyCb) {
            m_notifyCb(cmd, data, len);
        }
    }

    std::vector<uint8_t> m_lastWrite;
    int m_writeCount = 0;
    bool m_connected = false;
    NotifyCallback m_notifyCb;
};

TEST_CASE("G1Protocol init sends correct packet", "[protocol]") {
    TestTransport transport;
    G1Protocol protocol(transport);

    transport.connect("");
    bool result = protocol.init();

    REQUIRE(result);
    REQUIRE(transport.m_writeCount >= 1);
    REQUIRE(transport.m_lastWrite[0] == CMD_INIT);
}

TEST_CASE("G1Protocol heartbeat sends correct packet", "[protocol]") {
    TestTransport transport;
    G1Protocol protocol(transport);

    transport.connect("");
    protocol.heartbeat();

    REQUIRE(transport.m_writeCount >= 1);
    REQUIRE(transport.m_lastWrite[0] == 0x25);  // HEARTBEAT
}

TEST_CASE("G1Protocol sendText builds correct packet", "[protocol]") {
    TestTransport transport;
    G1Protocol protocol(transport);

    transport.connect("");
    protocol.sendText("Hi", 1, 1);

    REQUIRE(transport.m_writeCount >= 1);
    REQUIRE(transport.m_lastWrite[0] == CMD_TEXT);
    // Payload: page(1) + maxPage(1) + "Hi"(2) = 4 bytes
    REQUIRE(transport.m_lastWrite[1] == 0x04);
    REQUIRE(transport.m_lastWrite[2] == 0x00);
    REQUIRE(transport.m_lastWrite[3] == 0x01);  // page
    REQUIRE(transport.m_lastWrite[4] == 0x01);  // maxPage
    REQUIRE(transport.m_lastWrite[5] == 'H');
    REQUIRE(transport.m_lastWrite[6] == 'i');
}

TEST_CASE("G1Protocol exit sends correct packet", "[protocol]") {
    TestTransport transport;
    G1Protocol protocol(transport);

    transport.connect("");
    protocol.exit();

    REQUIRE(transport.m_writeCount >= 1);
    REQUIRE(transport.m_lastWrite[0] == CMD_EXIT);
}

TEST_CASE("G1Protocol sendBmp sends CRC + data + end", "[protocol]") {
    TestTransport transport;
    G1Protocol protocol(transport);

    transport.connect("");

    uint8_t pixelData[20] = {0};
    uint8_t addr[4] = {0x00, 0x1C, 0x00, 0x00};

    protocol.sendBmp(pixelData, sizeof(pixelData), addr, 0);

    // Should have: CRC packet + BMP_DATA packet + BMP_END packet
    REQUIRE(transport.m_writeCount >= 3);
}

TEST_CASE("G1Protocol response callback works", "[protocol]") {
    TestTransport transport;
    G1Protocol protocol(transport);

    transport.connect("");

    uint8_t receivedCmd = 0;
    uint8_t receivedData[16] = {0};
    uint16_t receivedLen = 0;

    protocol.setResponseCallback([&](uint8_t cmd, const uint8_t* data, uint16_t len) {
        receivedCmd = cmd;
        receivedLen = len;
        if (len <= 16) {
            std::memcpy(receivedData, data, len);
        }
    });

    // Simulate incoming response
    uint8_t responseData[] = {0x01, 0x02, 0x03};
    transport.simulateResponse(RSP_SUCCESS, responseData, sizeof(responseData));

    REQUIRE(receivedCmd == RSP_SUCCESS);
    REQUIRE(receivedLen == 3);
    REQUIRE(receivedData[0] == 0x01);
    REQUIRE(receivedData[1] == 0x02);
    REQUIRE(receivedData[2] == 0x03);
}
