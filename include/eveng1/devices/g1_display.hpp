#pragma once

#include "../eveng1.hpp"
#include "../devices/device.hpp"
#include "../protocol/g1_protocol.hpp"
#include <memory>
#include <string>
#include <functional>

namespace eveng1 {

class G1Display {
public:
    explicit G1Display(std::unique_ptr<BleTransport> transport);

    // Connection
    bool connect(const char* address);
    void disconnect();
    bool isConnected() const;

    // Display capabilities
    uint32_t width() const { return DISPLAY_WIDTH; }
    uint32_t height() const { return DISPLAY_HEIGHT; }

    // Drawing (operates on internal framebuffer)
    void clear(bool value = false);
    void fillRect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, bool value = true);
    void drawLine(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1);
    void drawText(uint16_t x, uint16_t y, const char* text, RenderStyle style = {});

    // Present framebuffer to glasses
    bool present(Dither dither = Dither::NONE, bool invert = false);

    // Text-only display (uses 0x4E protocol, faster)
    bool showText(const char* text, uint8_t page = 1, uint8_t maxPage = 1);

    // Heartbeat (call periodically to keep connection alive)
    bool heartbeat();

    // Input events
    void setInputCallback(InputCallback cb);

    // Direct access to framebuffer for custom rendering
    Framebuffer& framebuffer() { return m_fb; }
    const Framebuffer& framebuffer() const { return m_fb; }

private:
    std::unique_ptr<BleTransport> m_transport;
    std::unique_ptr<G1Protocol> m_protocol;
    Framebuffer m_fb;
    DirtyRect m_dirty;
    InputCallback m_inputCb;
};

}  // namespace eveng1
