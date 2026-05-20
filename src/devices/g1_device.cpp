#include "eveng1/devices/device.hpp"
#include "eveng1/protocol/g1_commands.hpp"

namespace eveng1 {

class G1Device : public DisplayDevice {
public:
    explicit G1Device(BleTransport& transport) : m_transport(transport) {}

    bool connect() override {
        return m_transport.connect("");
    }

    void disconnect() override {
        m_transport.disconnect();
    }

    bool isConnected() const override {
        return m_transport.isConnected();
    }

    uint32_t width() const override { return DISPLAY_WIDTH; }
    uint32_t height() const override { return DISPLAY_HEIGHT; }
    bool supportsTextProtocol() const override { return true; }
    bool supportsPartialUpdate() const override { return false; }

    void sendText(const char* text, uint8_t page, uint8_t maxPage) override {}
    void sendBitmap(const Framebuffer& fb, const DirtyRect& rect) override {}

    void setInputCallback(InputCallback cb) override { m_inputCb = cb; }
    void heartbeat() override {}
    void poll() override {}

private:
    BleTransport& m_transport;
    InputCallback m_inputCb;
};

}  // namespace eveng1
