#pragma once

#include "../eveng1.hpp"
#include <memory>
#include <functional>
#include <string>
#include <cstdint>

namespace eveng1 {

// ─── Abstract Transport ───

class BleTransport {
public:
    virtual ~BleTransport() = default;

    using NotifyCallback = std::function<void(uint8_t cmd, const uint8_t* data,
                                                uint16_t len)>;

    virtual bool connect(const char* address) = 0;
    virtual void disconnect() = 0;
    virtual bool isConnected() const = 0;

    /// Write data to the BLE TX characteristic.
    virtual bool write(const uint8_t* data, uint16_t len) = 0;

    /// Set callback for incoming notifications.
    virtual void setNotifyCallback(NotifyCallback cb) = 0;

    /// Wait for an incoming notification (blocking, with timeout).
    /// Returns received data, or nullptr on timeout.
    virtual uint8_t* waitForNotify(uint16_t& outLen, uint32_t timeoutMs) = 0;
};

// ─── Display Device Interface ───

class DisplayDevice {
public:
    virtual ~DisplayDevice() = default;

    virtual bool connect() = 0;
    virtual void disconnect() = 0;
    virtual bool isConnected() const = 0;

    virtual uint32_t width() const = 0;
    virtual uint32_t height() const = 0;
    virtual bool supportsTextProtocol() const = 0;
    virtual bool supportsPartialUpdate() const = 0;

    virtual void sendText(const char* text, uint8_t page, uint8_t maxPage) = 0;
    virtual void sendBitmap(const Framebuffer& fb, const DirtyRect& rect) = 0;

    virtual void setInputCallback(InputCallback cb) = 0;

    virtual void heartbeat() = 0;
    virtual void poll() = 0;
};

}  // namespace eveng1
