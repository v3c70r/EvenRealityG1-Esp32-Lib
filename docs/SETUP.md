# Development Environment Setup

## Prerequisites

- **ESP32-S3** board (recommended: `esp32-s3-devkitc-1`)
- USB-C cable for programming
- PlatformIO CLI or VS Code + PlatformIO extension
- Even Reality G1 glasses (both arms, unpaired from phone)

## Install PlatformIO

```bash
# Via pip
pip install platformio

# Or VS Code extension: PlatformIO IDE
# https://marketplace.visualstudio.com/items?itemName=platformio.platformio-ide
```

## Desktop Build (for unit tests only)

```bash
# Requires: cmake, C++17 compiler, Catch2 (auto-downloaded)
cmake -B build -S .
cmake --build build -j$(nproc)
ctest --test-dir build   # 8 tests
```

## ESP32 Projects

This repo contains two ESP32 projects:

| Project | Directory | Purpose |
|---------|-----------|---------|
| **bmp_tester** | `examples/esp32/bmp_tester` | Protocol validation (T1–T9 BMP, T10–T15 text) |
| **g1_debug** | `examples/esp32/g1_debug` | Interactive debug tool with menu |

### Build & Upload

```bash
cd examples/esp32/g1_debug
pio run -t upload --upload-port /dev/ttyACM0
```

### Monitor Serial Output

```bash
# Requires dialout group permission
sudo usermod -a -G dialout $USER
# Log out and back in, then:
pio device monitor --baud 115200
```

### Serial Permission (Linux)

```bash
# One-time setup
sudo usermod -a -G dialout $USER
# Log out and log back in

# Or use sg for one-off access:
sg dialout -c "pio run -t upload --upload-port /dev/ttyACM0"
```

## Project Structure

```
EvenG1Lib/
├── include/eveng1/          # Library headers
│   ├── eveng1.hpp           # Core types (Framebuffer, Font, RenderStyle)
│   ├── devices/
│   │   ├── device.hpp       # BleTransport + DisplayDevice interfaces
│   │   └── g1_display.hpp   # G1Display high-level API
│   ├── graphics/
│   │   ├── rasterizer.hpp   # Dithering (ordered + Floyd-Steinberg)
│   │   └── crc32_xz.hpp     # CRC-32/XZ utilities
│   └── protocol/
│       ├── g1_commands.hpp  # All protocol constants
│       └── g1_protocol.hpp  # G1Protocol (BMP/text/settings)
├── src/                     # Library implementations
│   ├── devices/g1_display.cpp
│   ├── graphics/
│   ├── protocol/
│   └── transport/
├── tests/                   # Catch2 unit tests
├── examples/esp32/          # PlatformIO projects
│   ├── bmp_tester/          # Protocol validator
│   └── g1_debug/            # Interactive debug tool
├── docs/                    # Documentation
│   ├── API.md               # API reference
│   └── SETUP.md             # This file
└── CMakeLists.txt           # Desktop build system
```

## Minimal ESP32 Project

Create a new PlatformIO project:

```ini
# platformio.ini
[env:esp32-s3]
platform = espressif32
board = esp32-s3-devkitc-1
framework = arduino
lib_deps =
    h2zero/NimBLE-Arduino @ ^2.2.0
build_flags = -std=c++17
```

```cpp
// src/main.cpp
#include <Arduino.h>
#include "g1_ble.hpp"    // copy from examples/esp32/g1_debug/src/
#include "g1_commands.hpp"
#include "crc32.hpp"
#include "bmp_pattern.hpp"

eveng1::G1Device g_device;

void setup() {
    Serial.begin(115200);
    g_device.scanAndConnect(10000);
    g_device.start();
    g_device.sendText("Hello World!");
}

void loop() {
    static uint32_t last = 0;
    if (millis() - last > 8000) {
        g_device.heartbeat();
        last = millis();
    }
}
```

## Dependencies

| Dependency | Version | Required |
|------------|---------|----------|
| NimBLE-Arduino | ^2.2.0 | ESP32 BLE transport |
| Catch2 | ^3.7 | Desktop unit tests |
| D-Bus | 1.x | Linux BLE (removed, see history) |

## Troubleshooting

### Connection fails with err=517
Glasses are bonded to another device. Unpair from phone and reset glasses:
1. Open Even app → unpair/disconnect
2. Put glasses in case, close temples
3. Take out of case, scan again

### BMP not rendering
- Ensure `0xF4 0x01` init was sent
- Send CRC BEFORE BMP data
- Wait for `0xC9` success in BMP_END response
- Send `0x18` exit after BMP to trigger display refresh
- Avoid sending text (`0x4E`) before BMP — text mode blocks BMP rendering

### Left arm write failures
- Use 15ms inter-packet delay
- Send heartbeat between retry attempts
- ESP32-S3 performs better than WROOM-32 (RSSI)

### Stack overflow
- Large buffers (>8KB) must use heap allocation (`new[]`)
- ESP32 loopTask default stack is ~8KB
