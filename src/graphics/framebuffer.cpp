#include "eveng1/eveng1.hpp"
#include <cstring>
#include <algorithm>

namespace eveng1 {

// ─── Rect ───

Rect Rect::unionWith(const Rect& other) const {
    if (isEmpty()) return other;
    if (other.isEmpty()) return *this;
    uint16_t nx = std::min(x, other.x);
    uint16_t ny = std::min(y, other.y);
    uint16_t nw = std::max(x + width, other.x + other.width) - nx;
    uint16_t nh = std::max(y + height, other.y + other.height) - ny;
    return {nx, ny, nw, nh};
}

bool Rect::contains(uint16_t px, uint16_t py) const {
    return px >= x && px < x + width && py >= y && py < y + height;
}

// ─── DirtyRect ───

void DirtyRect::expand(uint16_t px, uint16_t py) {
    if (isEmpty()) {
        x = px; y = py; width = 1; height = 1;
    } else {
        uint16_t nx = std::min(x, px);
        uint16_t ny = std::min(y, py);
        uint16_t nw = std::max(x + width, px + 1) - nx;
        uint16_t nh = std::max(y + height, py + 1) - ny;
        x = nx; y = ny; width = nw; height = nh;
    }
}

void DirtyRect::expand(const Rect& r) {
    if (r.isEmpty()) return;
    if (isEmpty()) {
        x = r.x; y = r.y; width = r.width; height = r.height;
    } else {
        uint16_t nx = std::min(x, r.x);
        uint16_t ny = std::min(y, r.y);
        uint16_t nw = std::max(x + width, r.x + r.width) - nx;
        uint16_t nh = std::max(y + height, r.y + r.height) - ny;
        x = nx; y = ny; width = nw; height = nh;
    }
}

void DirtyRect::reset() {
    x = 0; y = 0; width = 0; height = 0;
}

// ─── Framebuffer ───

Framebuffer::Framebuffer() {
    data.resize(ROW_BYTES * DEFAULT_HEIGHT, 0);
}

Framebuffer::Framebuffer(uint32_t w, uint32_t h)
    : width(w), height(h) {
    uint32_t rowBytes = (w + 7) / 8;  // Round up to nearest byte
    data.resize(h * rowBytes, 0);
}

void Framebuffer::clear(bool value) {
    std::fill(data.begin(), data.end(), value ? 0xFF : 0x00);
}

void Framebuffer::setPixel(uint16_t x, uint16_t y, bool on) {
    if (x >= width || y >= height) return;
    uint32_t rowBytes = (width + 7) / 8;
    size_t idx = y * rowBytes + x / 8;
    int bit = 7 - (x % 8);
    if (on)
        data[idx] |= (1 << bit);
    else
        data[idx] &= ~(1 << bit);
}

bool Framebuffer::getPixel(uint16_t x, uint16_t y) const {
    if (x >= width || y >= height) return false;
    uint32_t rowBytes = (width + 7) / 8;
    size_t idx = y * rowBytes + x / 8;
    int bit = 7 - (x % 8);
    return (data[idx] >> bit) & 1;
}

void Framebuffer::fillRect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, bool value) {
    if (x >= width || y >= height) return;
    w = std::min(w, static_cast<uint16_t>(width - x));
    h = std::min(h, static_cast<uint16_t>(height - y));

    uint32_t rowBytes = (width + 7) / 8;

    for (uint16_t row = y; row < y + h; row++) {
        size_t base = row * rowBytes;
        uint16_t xEnd = x + w;

        // Handle partial start byte
        uint16_t startByte = x / 8;
        uint16_t endByte = (xEnd - 1) / 8;

        if (startByte == endByte) {
            uint8_t mask = 0;
            for (uint16_t px = x; px < xEnd; px++) {
                mask |= (1 << (7 - (px % 8)));
            }
            if (value)
                data[base + startByte] |= mask;
            else
                data[base + startByte] &= ~mask;
        } else {
            // Start byte
            uint8_t startMask = 0;
            for (uint16_t px = x; px < (startByte + 1) * 8; px++) {
                startMask |= (1 << (7 - (px % 8)));
            }
            if (value)
                data[base + startByte] |= startMask;
            else
                data[base + startByte] &= ~startMask;

            // Middle bytes
            uint8_t fill = value ? 0xFF : 0x00;
            for (uint16_t b = startByte + 1; b < endByte; b++) {
                data[base + b] = fill;
            }

            // End byte
            uint8_t endMask = 0;
            for (uint16_t px = endByte * 8; px < xEnd; px++) {
                endMask |= (1 << (7 - (px % 8)));
            }
            if (value)
                data[base + endByte] |= endMask;
            else
                data[base + endByte] &= ~endMask;
        }
    }
}

void Framebuffer::drawLine(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1) {
    int dx = std::abs(static_cast<int>(x1) - static_cast<int>(x0));
    int dy = -std::abs(static_cast<int>(y1) - static_cast<int>(y0));
    int sx = (x0 < x1) ? 1 : -1;
    int sy = (y0 < y1) ? 1 : -1;
    int err = dx + dy;

    while (true) {
        setPixel(x0, y0, true);
        if (x0 == x1 && y0 == y1) break;
        int e2 = 2 * err;
        if (e2 >= dy) { err += dy; x0 += sx; }
        if (e2 <= dx) { err += dx; y0 += sy; }
    }
}

void Framebuffer::blitGlyph(uint16_t x, uint16_t y,
                             const uint8_t* glyphData, uint8_t glyphW, uint8_t glyphH,
                             RenderStyle style) {
    for (uint8_t gy = 0; gy < glyphH; gy++) {
        uint8_t row = glyphData[gy];
        for (uint8_t gx = 0; gx < glyphW; gx++) {
            bool pixel = (row >> (7 - gx)) & 1;
            if (style.invert) pixel = !pixel;
            if (pixel)
                setPixel(x + gx, y + gy, true);
        }
    }
}

}  // namespace eveng1
