#include "eveng1/devices/device.hpp"
#include "eveng1/protocol/g1_commands.hpp"

namespace eveng1 {

class SimulatorDevice : public DisplayDevice {
public:
    SimulatorDevice() : m_connected(false), m_fb(DISPLAY_WIDTH, DISPLAY_HEIGHT) {}

    bool connect() override { m_connected = true; return true; }
    void disconnect() override { m_connected = false; }
    bool isConnected() const override { return m_connected; }

    uint32_t width() const override { return DISPLAY_WIDTH; }
    uint32_t height() const override { return DISPLAY_HEIGHT; }
    bool supportsTextProtocol() const override { return true; }
    bool supportsPartialUpdate() const override { return true; }

    void sendText(const char* text, uint8_t page, uint8_t maxPage) override {}
    void sendBitmap(const Framebuffer& fb, const DirtyRect& rect) override {
        m_fb = fb;
    }

    void setInputCallback(InputCallback cb) override { m_inputCb = cb; }
    void heartbeat() override {}
    void poll() override {}

    const Framebuffer& getFramebuffer() const { return m_fb; }

private:
    bool m_connected;
    Framebuffer m_fb;
    InputCallback m_inputCb;
};

}  // namespace eveng1
