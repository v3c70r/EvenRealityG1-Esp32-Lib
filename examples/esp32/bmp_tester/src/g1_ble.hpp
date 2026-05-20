#pragma once
#include <NimBLEDevice.h>
#include <functional>
#include <queue>
#include <vector>
#include <string>
#include <cstdint>

namespace eveng1 {

/// Callback for incoming notifications: (cmd_byte, data, data_len)
using NotifyCallback = std::function<void(uint8_t cmd, const uint8_t* data, uint16_t len)>;

/// Manages a single BLE connection (left or right arm)
class G1Side {
public:
    G1Side(const char* sideName) : _name(sideName) {}
    ~G1Side();

    bool connect(NimBLEAdvertisedDevice* device);
    void disconnect();

    bool isConnected() const {
        return _client != nullptr && _client->isConnected();
    }

    uint16_t getMTU() const {
        return _client ? _client->getMTU() : 23;
    }

    /// Write data to TX characteristic (response=false for Write Without Response)
    bool write(const uint8_t* data, uint16_t len, bool response = false);

    /// Wait for a notification with specific command byte
    /// Returns raw notification data, or nullptr on timeout
    struct NotifyPacket {
        uint8_t cmd;
        std::vector<uint8_t> data;
    };

    bool waitForNotify(uint8_t expectedCmd, NotifyPacket& out, uint32_t timeoutMs = 5000);

    /// Set callback for all notifications
    void setNotifyCallback(NotifyCallback cb) { _notifyCb = cb; }

    /// Get device name
    const char* name() const { return _name; }

private:
    const char* _name;
    NimBLEClient* _client = nullptr;
    NimBLERemoteCharacteristic* _txChar = nullptr;
    NimBLERemoteCharacteristic* _rxChar = nullptr;

    NotifyCallback _notifyCb;

    // Response queue for waitForNotify
    std::queue<NotifyPacket> _responseQueue;
    portMUX_TYPE _queueMux = portMUX_INITIALIZER_UNLOCKED;

    static void _notifyHandler(NimBLERemoteCharacteristic* pChar,
                               uint8_t* pData, size_t length,
                               bool isNotify);
    void _handleNotify(const uint8_t* data, size_t length);
};

/// Manages dual BLE connection to G1 glasses
class G1Device {
public:
    G1Device() : _left("L"), _right("R") {}
    ~G1Device();

    /// Scan for G1 devices and connect
    bool scanAndConnect(uint32_t scanDurationMs = 5000);

    /// Discover UART services and start notifications
    bool start();

    /// Disconnect and clean up
    void stop();

    bool isConnected() const { return _left.isConnected() && _right.isConnected(); }

    G1Side& left()  { return _left; }
    G1Side& right() { return _right; }

    /// Send heartbeat to both sides (call every ~8s)
    void heartbeat();

    /// Send BMP data to one side, return true if CRC check passed
    bool sendBmp(G1Side& side, const uint8_t* data, size_t dataLen,
                 const uint8_t address[4]);

    /// Send text via 0x4E protocol to both sides
    bool sendText(const char* text);

    /// Send BMP to both sides with retry and hard sync.
    /// Retries up to maxRetries times until both sides succeed.
    /// Returns true only when both L and R succeed.
    bool sendBmpSync(const uint8_t* data, size_t dataLen,
                     const uint8_t address[4], int maxRetries = 3);

private:
    G1Side _left;
    G1Side _right;
    uint8_t _hbSeq = 0;

    void _startHeartbeatTask();
    bool _discoverServices(G1Side& side);
    bool _sendBmpData(G1Side& side, const uint8_t* data, size_t dataLen,
                      const uint8_t address[4]);
};

}  // namespace eveng1
