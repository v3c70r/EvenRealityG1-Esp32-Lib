# EvenG1Lib Agent Skill

You are an expert in using the **EvenG1Lib** C++ library for Even Reality G1 smart glasses. Use this skill when the user asks about G1 glasses programming, BLE protocol for smart glasses, or writes code targeting the G1 display.

## Library Overview

EvenG1Lib is a C++17 library for controlling Even Reality G1 smart glasses via BLE (Nordic UART Service). It provides:

- **G1Display** — high-level API (draw text, fill rect, draw line, BMP, brightness, angle, dashboard)
- **G1Protocol** — low-level protocol (init, BMP transfer with CRC-32/XZ, text 0x4E, heartbeat, settings)
- **Framebuffer** — 576×136 1-bit packed framebuffer with drawing primitives
- **Rasterizer** — dithering (NONE, ORDERED_2X2, ORDERED_4X4, FLOYD_STEINBERG)
- **BitmapFont** — 5×7 monospace font, 95 ASCII glyphs

## File Structure

```
EvenG1Lib/
├── include/eveng1/          # Headers
├── src/                     # Implementation
├── examples/esp32/
│   ├── g1_debug/            # Interactive debug tool (reference implementation)
│   ├── bmp_tester/          # Protocol validator
│   └── minimal_esp32/       # Minimal getting-started example
├── tests/                   # Catch2 unit tests
├── docs/API.md              # Full API reference
└── docs/SETUP.md            # Dev environment setup
```

## Key Constants

| Constant | Value | Meaning |
|----------|-------|---------|
| `DISPLAY_WIDTH` | 576 | Pixels wide |
| `DISPLAY_HEIGHT` | 136 | Pixels tall |
| `DISPLAY_ROW_BYTES` | 72 | Bytes per row (576/8) |
| `FULL_FB_SIZE` | 9792 | Full framebuffer in bytes |
| `BMP_PACKET_SIZE` | 194 | Pixel data per BLE packet |
| `BMP_BASE_ADDRESS` | 0x001C0000 | VRAM base address |
| `BRIGHTNESS_MAX` | 63 | Max brightness level |
| `HEAD_UP_ANGLE_MAX` | 60 | Max head-up angle degrees |
| `BMP_ADDR_BYTES` | {0x00,0x1C,0x00,0x00} | Base address bytes |

## Platform: ESP32

This library targets **ESP32** with NimBLE-Arduino. Linux/desktop support was removed.

ESP32 projects use the local `g1_ble.hpp/cpp` implementation (not the abstract `BleTransport` from `include/eveng1/`). The local implementation uses:
- `NimBLEDevice` for BLE scanning and connection
- `NimBLEClient` for GATT operations
- NUS UUIDs: `6E400001-B5A3-F393-E0A9-E50E24DCCA9E` (service), `6E400002-...` (TX), `6E400003-...` (RX)

## ESP32 Build

```bash
cd examples/esp32/g1_debug
pio run -t upload --upload-port /dev/ttyACM0
```

PlatformIO config:
```ini
[env:esp32-s3]
platform = espressif32
board = esp32-s3-devkitc-1
framework = arduino
lib_deps = h2zero/NimBLE-Arduino @ ^2.2.0
build_flags = -std=c++17
```

## Writing G1 Code

### Connection Pattern

```cpp
G1Device g_device;

// Setup
g_device.scanAndConnect(10000);  // 10s scan
g_device.start();                // Init (0xF4 0x01)

// Heartbeat every 8s
g_device.heartbeat();

// Cleanup
g_device.stop();
```

### Display Modes

1. **Text mode** (`0x4E`): Fast, up to 5 lines, 488px wide
2. **BMP mode** (`0x15`/`0x20`): Full 576×136 rendering

Switching: Text mode blocks BMP. Send `0x18` (exit) to return to dashboard before sending BMP, or send BMP when no text is displayed. After BMP, `0x18` triggers display refresh.

### BMP Protocol Order (REQUIRED)

```
1. CRC packet  [0x16, crc[4]]          ← sent FIRST
2. BMP data    [0x15, seq, addr(4)?, pixelData[194]]
3. BMP end     [0x20, 0x0D, 0x0E]      ← triggers render + CRC check
4. Response    0xC9 = success
```

### CRC Computation

```cpp
#include "crc32.hpp"
uint8_t crc[4];
crc32_xz_bmp(BMP_ADDR_BYTES, pixelData, dataLen, crc);
// CRC is over address[4] + pixelData[]
// Output: big-endian bytes
// Always use BMP_ADDR_BYTES (base address) for CRC, not the region address
```

### Pattern Generation

```cpp
#include "bmp_pattern.hpp"

uint8_t fb[FULL_FB_SIZE];
generateCheckerboard(fb);       // 8x8 checkerboard
generateStripes(fb);            // 4-row horizontal stripes
generateTestPattern("T1", fb);  // Named test pattern

// Extract partial region
BmpRegion region{0, 20, 576, 50};  // x, y, w, h
size_t dataSize = region.dataSize();
uint8_t* data = new uint8_t[dataSize];
extractRegion(fb, region, data);
```

### Text Sending

```cpp
g_device.sendText("Hello G1!\nLine2\nLine3");
// \n for new lines, max 5 lines, 488px wide
// Uses 0x4E protocol with screen status 0x71 (text + new content)
```

### Settings Commands

```cpp
g_device.setBrightness(30);              // 0-63
g_device.setBrightness(-1, true);        // auto brightness
g_device.setHeadUpAngle(25);             // 0-60 degrees
g_device.setDashboardPosition(4, 5);     // height 0-8, depth 1-9
g_device.setWearDetection(false);        // keep display on
g_device.setSilentMode(false);
g_device.setMicEnabled(true);
g_device.queryBattery();                 // response via 0xF5 event
```

## Common Issues

### Stack overflow
ESP32 loopTask has ~8KB stack. Large buffers must use heap:
```cpp
// WRONG: uint8_t fb[9792];
// RIGHT: uint8_t* fb = new uint8_t[9792];
```

### BMP not rendering
1. Did you send CRC BEFORE BMP data?
2. Is text mode active? Send `0x18` first.
3. Try `g_device.sendExit()` after BMP to trigger refresh.
4. Always use `BMP_ADDR_BYTES` for CRC (not region address).

### Connection fails (err=517)
Glasses bonded to phone. Unpair from Even app and reset glasses in case.

### Left arm write failures
- Use 15ms inter-packet delay
- Send heartbeat between retries
- ESP32-S3 has better RSSI than WROOM-32

### Webcam verification
Regular webcams CANNOT reliably capture the waveguide display. The near-eye projection is designed for human eyes. Use direct visual verification through the lens.

## Testing

Desktop unit tests (8 suites, all passing):
```bash
cmake -B build -S . && cmake --build build && ctest --test-dir build
```

Tests cover: framebuffer, rasterizer, command_buffer, dirty_rect, dither, font, crc32, protocol.

## Reference Implementation

The most complete reference is `examples/esp32/g1_debug/` which implements:
- Full BLE connection lifecycle
- All protocol commands (BMP, text, settings, events)
- Interactive serial debug menu
- Touch/case/head event handling
- Battery monitoring
- Widget rendering (status bars) via BMP
