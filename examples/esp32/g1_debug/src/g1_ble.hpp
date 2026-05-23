#pragma once
#include <NimBLEDevice.h>
#include <functional>
#include <queue>
#include <vector>
#include <string>
#include <cstdint>

namespace eveng1 {

using NotifyCallback = std::function<void(uint8_t cmd, const uint8_t* data, uint16_t len)>;

// ─── Event Types ───

enum class G1Event : uint8_t {
    SINGLE_TAP_LEFT,
    SINGLE_TAP_RIGHT,
    DOUBLE_TAP_LEFT,
    DOUBLE_TAP_RIGHT,
    TRIPLE_TAP_LEFT,
    TRIPLE_TAP_RIGHT,
    LONG_PRESS_LEFT,
    LONG_PRESS_RIGHT,
    HEAD_UP,
    HEAD_DOWN,
    CASE_REMOVED,
    CASE_OPEN,
    CASE_CLOSED,
    CASE_CHARGING,
    BATTERY_UPDATE,
};

using EventCallback = std::function<void(G1Event event, const uint8_t* data, uint16_t len)>;

// ─── G1Side ───

class G1Side {
public:
    G1Side(const char* sideName) : _name(sideName) {}
    ~G1Side();

    bool connect(NimBLEAdvertisedDevice* device);
    void disconnect();
    bool isConnected() const { return _client != nullptr && _client->isConnected(); }
    const char* name() const { return _name; }

    bool write(const uint8_t* data, uint16_t len, bool response = false);

    struct NotifyPacket {
        uint8_t cmd;
        std::vector<uint8_t> data;
    };
    bool waitForNotify(uint8_t expectedCmd, NotifyPacket& out, uint32_t timeoutMs = 5000);
    void setNotifyCallback(NotifyCallback cb) { _notifyCb = cb; }

private:
    const char* _name;
    NimBLEClient* _client = nullptr;
    NimBLERemoteCharacteristic* _txChar = nullptr;
    NimBLERemoteCharacteristic* _rxChar = nullptr;
    NotifyCallback _notifyCb;
    std::queue<NotifyPacket> _responseQueue;
    portMUX_TYPE _queueMux = portMUX_INITIALIZER_UNLOCKED;
    void _handleNotify(const uint8_t* data, size_t length);
};

// ─── G1Device ───

class G1Device {
public:
    G1Device() : _left("L"), _right("R") {}
    ~G1Device();

    // Connection
    bool scanAndConnect(uint32_t scanDurationMs = 5000);
    bool start();
    void stop();
    bool isConnected() const { return _left.isConnected() && _right.isConnected(); }

    G1Side& left()  { return _left; }
    G1Side& right() { return _right; }

    // Protocol
    void heartbeat();
    bool sendBmp(G1Side& side, const uint8_t* data, size_t dataLen, const uint8_t address[4]);
    bool sendBmpSync(const uint8_t* data, size_t dataLen, const uint8_t address[4], int maxRetries = 3);
    bool sendText(const char* text);

    // Display settings
    bool setBrightness(uint8_t level, bool autoLight = false);
    bool setHeadUpAngle(uint8_t angle);
    bool setDashboardPosition(uint8_t height, uint8_t depth);

    // System
    bool queryBattery();
    bool setWearDetection(bool enabled);
    bool setSilentMode(bool enabled);
    bool setMicEnabled(bool enabled);
    bool sendExit();

    // Event system
    void setEventCallback(EventCallback cb) { _eventCb = cb; }
    int getLeftBattery()  const { return _batteryLeft; }
    int getRightBattery() const { return _batteryRight; }
    const char* getLastEvent() const { return _lastEvent; }

private:
    G1Side _left;
    G1Side _right;
    uint8_t _hbSeq = 0;

    EventCallback _eventCb = nullptr;
    int _batteryLeft = -1;
    int _batteryRight = -1;
    char _lastEvent[64] = "";

    bool _sendBmpData(G1Side& side, const uint8_t* data, size_t dataLen, const uint8_t address[4]);
    void _onNotification(uint8_t cmd, const uint8_t* data, uint16_t len);
    const char* _eventName(G1Event e);
};

}  // namespace eveng1
