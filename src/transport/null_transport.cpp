#include "eveng1/devices/device.hpp"
#include <cstring>

namespace eveng1 {

class NullTransport : public BleTransport {
public:
    bool connect(const char* address) override { return true; }
    void disconnect() override {}
    bool isConnected() const override { return true; }
    bool write(const uint8_t* data, uint16_t len) override {
        lastWrite.assign(data, data + len);
        return true;
    }
    void setNotifyCallback(NotifyCallback cb) override { notifyCb = cb; }
    uint8_t* waitForNotify(uint16_t& outLen, uint32_t timeoutMs) override {
        outLen = 0;
        return nullptr;
    }

    std::vector<uint8_t> lastWrite;
    NotifyCallback notifyCb = nullptr;
};

}  // namespace eveng1
