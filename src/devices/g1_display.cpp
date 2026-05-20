#include "eveng1/devices/g1_display.hpp"
#include "eveng1/eveng1.hpp"
#include "eveng1/graphics/rasterizer.hpp"
#include "eveng1/protocol/g1_commands.hpp"
#include <cstring>
#include <iostream>

namespace eveng1 {

G1Display::G1Display(std::unique_ptr<BleTransport> transport)
    : m_transport(std::move(transport)),
      m_protocol(std::make_unique<G1Protocol>(*m_transport)),
      m_fb(DISPLAY_WIDTH, DISPLAY_HEIGHT) {
    m_transport->setNotifyCallback(
        [this](uint8_t cmd, const uint8_t* data, uint16_t len) {
            if (cmd == CMD_TOUCHBAR && m_inputCb) {
                bool isLeft = false;
                TouchEvent event = parseTouchbarData(data, len, isLeft);
                m_inputCb(event, isLeft);
            }
        });
}

bool G1Display::connect(const char* address) {
    if (!m_transport->connect(address)) {
        std::cerr << "BLE connection failed" << std::endl;
        return false;
    }

    if (!m_protocol->init()) {
        std::cerr << "G1 protocol init failed" << std::endl;
        m_transport->disconnect();
        return false;
    }

    return true;
}

void G1Display::disconnect() {
    if (m_protocol) {
        m_protocol->exit();
    }
    if (m_transport) {
        m_transport->disconnect();
    }
}

bool G1Display::isConnected() const {
    return m_transport && m_transport->isConnected();
}

void G1Display::clear(bool value) {
    m_fb.clear(value);
    m_dirty.reset();
    m_dirty.expand(0, 0);
    m_dirty.expand(static_cast<uint16_t>(DISPLAY_WIDTH - 1),
                   static_cast<uint16_t>(DISPLAY_HEIGHT - 1));
}

void G1Display::fillRect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, bool value) {
    m_fb.fillRect(x, y, w, h, value);
    m_dirty.expand(x, y);
    m_dirty.expand(x + w - 1, y + h - 1);
}

void G1Display::drawLine(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1) {
    // Expand dirty rect to cover line endpoints
    m_dirty.expand(std::min(x0, x1), std::min(y0, y1));
    m_dirty.expand(std::max(x0, x1), std::max(y0, y1));
    m_fb.drawLine(x0, y0, x1, y1);
}

void G1Display::drawText(uint16_t x, uint16_t y, const char* text, RenderStyle style) {
    const auto& font = BitmapFont::defaultFont();
    uint16_t cursorX = x;

    while (*text) {
        const uint8_t* glyph = font.glyph(*text);
        if (glyph) {
            m_fb.blitGlyph(cursorX, y, glyph, font.glyphWidth, font.glyphHeight, style);
            m_dirty.expand(cursorX, y);
            m_dirty.expand(cursorX + font.glyphWidth - 1, y + font.glyphHeight - 1);
            cursorX += font.glyphWidth;
        }
        text++;
    }
}

bool G1Display::present(Dither dither, bool invert) {
    if (!isConnected()) return false;

    // If no dirty region, nothing to send
    if (m_dirty.isEmpty()) return true;

    // Align dirty rect to byte boundaries (8-pixel width)
    uint16_t dx = (m_dirty.x / 8) * 8;
    uint16_t dw = ((m_dirty.x + m_dirty.width + 7) / 8) * 8 - dx;
    uint16_t dy = m_dirty.y;
    uint16_t dh = m_dirty.height;

    // Clamp to display bounds
    dx = std::min(dx, static_cast<uint16_t>(DISPLAY_WIDTH));
    dw = std::min(dw, static_cast<uint16_t>(DISPLAY_WIDTH - dx));
    dy = std::min(dy, static_cast<uint16_t>(DISPLAY_HEIGHT));
    dh = std::min(dh, static_cast<uint16_t>(DISPLAY_HEIGHT - dy));

    // Extract region from framebuffer
    uint32_t srcRowBytes = (DISPLAY_WIDTH + 7) / 8;
    uint32_t dstRowBytes = (dw + 7) / 8;
    std::vector<uint8_t> regionData(dh * dstRowBytes);

    for (uint16_t row = 0; row < dh; row++) {
        uint16_t srcY = dy + row;
        if (srcY >= DISPLAY_HEIGHT) break;

        for (uint16_t col = 0; col < dstRowBytes; col++) {
            uint16_t srcX = dx + col * 8;
            if (srcX >= DISPLAY_WIDTH) break;

            // Pack 8 pixels into one byte
            uint8_t byte = 0;
            for (int bit = 0; bit < 8; bit++) {
                if (srcX + bit < DISPLAY_WIDTH) {
                    bool pixel = m_fb.getPixel(srcX + bit, srcY);
                    if (invert) pixel = !pixel;
                    if (pixel) {
                        byte |= (1 << (7 - bit));
                    }
                }
            }
            regionData[row * dstRowBytes + col] = byte;
        }
    }

    // Calculate storage address for this region
    uint8_t addr[4] = {
        static_cast<uint8_t>((dy * DISPLAY_WIDTH + dx) / 8 >> 24),
        static_cast<uint8_t>((dy * DISPLAY_WIDTH + dx) / 8 >> 16),
        static_cast<uint8_t>((dy * DISPLAY_WIDTH + dx) / 8 >> 8),
        static_cast<uint8_t>((dy * DISPLAY_WIDTH + dx) / 8)
    };

    bool result = m_protocol->sendBmp(regionData.data(),
                                       static_cast<uint32_t>(regionData.size()),
                                       addr, 15);

    // Reset dirty rect after successful present
    if (result) {
        m_dirty.reset();
    }

    return result;
}

bool G1Display::showText(const char* text, uint8_t page, uint8_t maxPage) {
    if (!isConnected()) return false;
    return m_protocol->sendText(text, page, maxPage);
}

bool G1Display::heartbeat() {
    if (!isConnected()) return false;
    return m_protocol->heartbeat();
}

void G1Display::setInputCallback(InputCallback cb) {
    m_inputCb = cb;
}

}  // namespace eveng1
