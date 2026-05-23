# EvenG1Lib API Reference

C++17 library for Even Reality G1 smart glasses. Provides BLE protocol, BMP rendering, text display, and device settings over Nordic UART Service.

## Architecture

```
DisplayDevice (abstract)
  └── G1Display ─── G1Protocol ─── BleTransport (abstract)
       │                                │
       ├── Framebuffer, Font, CRC       ├── ESP32: NimBLE-Arduino
       └── Rasterizer, RenderStyle      └── Desktop: NullTransport (testing)
```

## Quick Start

```cpp
#include "eveng1/devices/g1_display.hpp"
#include "eveng1/devices/device.hpp"

auto transport = std::make_unique<YourBleTransport>();
eveng1::G1Display display(std::move(transport));

display.connect("AA:BB:CC:DD:EE:FF");
display.drawText(10, 10, "Hello G1!");
display.present();
display.heartbeat();
```

## Core Types (`eveng1.hpp`)

| Type | Description |
|------|-------------|
| `Framebuffer` | 576×136 1-bit packed framebuffer. `setPixel`, `fillRect`, `drawLine`, `blitGlyph` |
| `BitmapFont` | 5×7 monospace font, 95 printable ASCII glyphs. `glyph(c)`, `measureWidth(text)` |
| `RenderStyle` | `{Dither dither, bool invert}` — dither mode + inversion |
| `DirtyRect` | Accumulates drawn regions for partial updates |
| `Dither` | `NONE`, `ORDERED_2X2`, `ORDERED_4X4`, `FLOYD_STEINBERG` |
| `TouchEvent` | Touchbar events: single/double/triple tap, long press, wearing/not |

## G1Display API

### Connection

```cpp
bool connect(const char* address);  // BLE MAC address
void disconnect();
bool isConnected() const;
```

### Drawing (on internal framebuffer)

```cpp
void clear(bool value = false);
void fillRect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, bool value = true);
void drawLine(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1);
void drawText(uint16_t x, uint16_t y, const char* text, RenderStyle style = {});
```

### Present (send to glasses)

```cpp
bool present(Dither dither = Dither::NONE, bool invert = false);  // BMP via BLE
bool showText(const char* text, uint8_t page = 1, uint8_t maxPage = 1); // 0x4E protocol
```

### Display Settings

```cpp
bool setBrightness(uint8_t level, bool autoLight = false);  // level: 0–63
bool setHeadUpAngle(uint8_t angle);                          // angle: 0–60 degrees
bool setDashboardPosition(uint8_t height, uint8_t depth);    // h:0–8, d:1–9
```

`setDashboardPosition` controls the virtual display position — height in the lens and perceived depth/distance.

### System

```cpp
bool heartbeat();                  // Keep connection alive (send every 8s)
bool queryBattery();               // Request battery level
bool setWearDetection(bool on);    // Enable/disable IR wear sensor
bool setSilentMode(bool on);       // Enable/disable notification sounds
```

### Input

```cpp
void setInputCallback(InputCallback cb);  // Receive touch events
// InputCallback = std::function<void(TouchEvent event, bool isLeft)>
```

### Direct Framebuffer Access

```cpp
Framebuffer& framebuffer();
const Framebuffer& framebuffer() const;
```

## Protocol Commands

| Command | Code | Direction | Description |
|---------|------|-----------|-------------|
| INIT | `0xF4 0x01` | → | Initialize display |
| HEARTBEAT | `0x25` 6B | → | Keep-alive (every 8s) |
| BMP_DATA | `0x15` | → | BMP pixel data packets |
| BMP_END | `0x20 0x0D 0x0E` | → | End of BMP transfer |
| CRC | `0x16` 5B | → | CRC-32/XZ check (before BMP) |
| TEXT | `0x4E` 9B header | → | Text display protocol |
| EXIT | `0x18` | → | Return to dashboard |
| BRIGHTNESS | `0x01` 3B | → | `[level(0-63), auto(0/1)]` |
| HEAD_ANGLE | `0x0B` 3B | → | `[angle(0-60), 0x01]` |
| DASHBOARD | `0x26` 8B | → | `[0x08,0x00,cnt,0x02,0x01,h,d]` |
| BATTERY | `0x2C 0x01` | → | Query battery |
| WEAR_DETECT | `0x27` 2B | → | `[enabled]` |
| SILENT | `0x03` 2B | → | `[enabled]` |
| MIC | `0x0E` 2B | → | `[enabled]` |
| TOUCHBAR | `0xF5` | ← | Touch/Case/Head events |
| BATTERY_RSP | `0x2C 0x66` | ← | `[0x2C, 0x66, level]` |

## Touch/Case Events (`0xF5` sub-codes)

| Sub-code | Event |
|----------|-------|
| `0x00` | Double tap |
| `0x01` | Single tap |
| `0x04` | Triple tap (left) |
| `0x05` | Triple tap (right) |
| `0x17` | Long press |
| `0x02` | Head up |
| `0x03` | Head down |
| `0x06`/`0x07` | Off face / case removed |
| `0x08` | Case opened |
| `0x0B` | Case closed |
| `0x0E` | Case charging status |
| `0x0F` | Case battery info |

## BLE Transport Interface

Implement `BleTransport` for your platform:

```cpp
class BleTransport {
public:
    virtual bool connect(const char* address) = 0;
    virtual void disconnect() = 0;
    virtual bool isConnected() const = 0;
    virtual bool write(const uint8_t* data, uint16_t len) = 0;
    virtual void setNotifyCallback(NotifyCallback cb) = 0;
    virtual uint8_t* waitForNotify(uint16_t& outLen, uint32_t timeoutMs) = 0;
};
```

ESP32 reference implementation: `examples/esp32/g1_debug/src/g1_ble.hpp`

## Display Specs

- Resolution: 576×136 pixels, 1-bit monochrome
- VRAM address: `0x001C0000` (big-endian: `00 1C 00 00`)
- Row stride: 72 bytes (`DISPLAY_ROW_BYTES`)
- Full framebuffer: 9792 bytes (`576/8 × 136`)
- BMP packet payload: 194 bytes
- Text display width: 488 pixels, 5 lines per screen
- Inter-packet delay: 15ms (ESP32), 8ms (iOS), 5ms (Android)

## CRC-32/XZ

- Polynomial: `0x04C11DB7`
- Init: `0xFFFFFFFF`, XorOut: `0xFFFFFFFF`
- Reflected input and output
- Test vector: `"123456789"` → `0xCBF43926`
- BMP CRC: computed over `address[4] + pixelData[]`, big-endian output
- **Must be sent BEFORE BMP data** — the BMP_END response carries the CRC check result
