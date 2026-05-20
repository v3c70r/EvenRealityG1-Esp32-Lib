#include "eveng1/devices/device.hpp"

namespace eveng1 {

// Linux BlueZ BLE transport - stub for Phase 1
// Full implementation in Phase 2

class BleBlueZ : public BleTransport {
public:
    bool connect(const char* address) override { return false; }
    void disconnect() override {}
    bool isConnected() const override { return false; }
    bool write(const uint8_t* data, uint16_t len) override { return false; }
    void setNotifyCallback(NotifyCallback cb) override {}
    uint8_t* waitForNotify(uint16_t& outLen, uint32_t timeoutMs) override {
        outLen = 0;
        return nullptr;
    }
};

}  // namespace eveng1
