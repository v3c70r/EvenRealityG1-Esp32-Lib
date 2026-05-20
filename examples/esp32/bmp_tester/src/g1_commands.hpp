#pragma once
#include <cstdint>
#include <cstddef>

namespace eveng1 {

// ─── G1 Protocol Command Constants ───

constexpr uint8_t CMD_INIT       = 0xF4;
constexpr uint8_t CMD_HEARTBEAT  = 0x25;
constexpr uint8_t CMD_BMP_DATA   = 0x15;
constexpr uint8_t CMD_CRC        = 0x16;
constexpr uint8_t CMD_EXIT       = 0x18;
constexpr uint8_t CMD_BMP_END    = 0x20;
constexpr uint8_t CMD_TEXT       = 0x4E;
constexpr uint8_t CMD_MIC        = 0x0E;
constexpr uint8_t CMD_TOUCHBAR   = 0xF5;
constexpr uint8_t CMD_MIC_DATA   = 0xF1;

constexpr uint8_t RSP_SUCCESS    = 0xC9;
constexpr uint8_t RSP_FAILURE    = 0xCA;

// Nordic UART Service UUIDs
constexpr const char* NUS_SERVICE_UUID = "6E400001-B5A3-F393-E0A9-E50E24DCCA9E";
constexpr const char* NUS_TX_CHAR_UUID = "6E400002-B5A3-F393-E0A9-E50E24DCCA9E";
constexpr const char* NUS_RX_CHAR_UUID = "6E400003-B5A3-F393-E0A9-E50E24DCCA9E";

// Display
constexpr uint32_t DISPLAY_WIDTH      = 576;
constexpr uint32_t DISPLAY_HEIGHT     = 136;
constexpr uint32_t DISPLAY_ROW_BYTES  = DISPLAY_WIDTH / 8;  // 72

// BMP defaults
constexpr uint32_t BMP_PACKET_SIZE    = 194;
constexpr uint32_t BMP_BASE_ADDRESS   = 0x001C0000;  // big-endian
constexpr uint8_t  BMP_ADDR_BYTES[4]  = {0x00, 0x1C, 0x00, 0x00};

// Heartbeat
constexpr uint8_t HEARTBEAT_CMD       = 0x25;
constexpr uint8_t HEARTBEAT_LEN       = 0x06;
constexpr uint32_t HEARTBEAT_INTERVAL_MS = 8000;

// Response timeout
constexpr uint32_t RESPONSE_TIMEOUT_MS = 5000;

// TouchBar sub-commands
constexpr uint8_t TOUCH_DOUBLE_TAP   = 0x00;
constexpr uint8_t TOUCH_SINGLE_TAP   = 0x01;
constexpr uint8_t TOUCH_TRIPLE_TAP_L = 0x04;
constexpr uint8_t TOUCH_TRIPLE_TAP_R = 0x05;
constexpr uint8_t TOUCH_LONG_PRESS   = 0x17;
constexpr uint8_t TOUCH_RELEASE      = 0x18;

// Screen status composition
constexpr uint8_t SCREEN_TEXT        = 0x71;  // 0x70 | 0x01
constexpr uint8_t SCREEN_AI_AUTO     = 0x31;  // 0x30 | 0x01

}  // namespace eveng1
