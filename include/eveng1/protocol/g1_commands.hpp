#pragma once

#include <cstdint>
#include <cstddef>

namespace eveng1 {

// G1 protocol command constants
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

// Response
constexpr uint8_t RSP_SUCCESS    = 0xC9;
constexpr uint8_t RSP_FAILURE    = 0xCA;

// Nordic UART Service
constexpr const char* NUS_SERVICE_UUID = "6E400001-B5A3-F393-E0A9-E50E24DCCA9E";
constexpr const char* NUS_TX_CHAR_UUID = "6E400002-B5A3-F393-E0A9-E50E24DCCA9E";
constexpr const char* NUS_RX_CHAR_UUID = "6E400003-B5A3-F393-E0A9-E50E24DCCA9E";

// Display
constexpr uint32_t DISPLAY_WIDTH  = 576;
constexpr uint32_t DISPLAY_HEIGHT = 136;
constexpr uint32_t DISPLAY_ROW_BYTES = DISPLAY_WIDTH / 8;  // 72

// BMP
constexpr uint32_t BMP_PACKET_SIZE = 194;
constexpr uint8_t  BMP_STORAGE_ADDRESS[4] = {0x00, 0x1C, 0x00, 0x00};

// Text
constexpr uint32_t TEXT_DISPLAY_WIDTH = 488;
constexpr uint8_t  TEXT_LINES_PER_SCREEN = 5;

// Heartbeat
constexpr uint8_t HEARTBEAT_TEMPLATE[] = {0x25, 0x06, 0x00, 0x00, 0x04, 0x00};

// TouchBar sub-commands
constexpr uint8_t TOUCH_DOUBLE_TAP   = 0x00;
constexpr uint8_t TOUCH_SINGLE_TAP   = 0x01;
constexpr uint8_t TOUCH_TRIPLE_TAP_L = 0x04;
constexpr uint8_t TOUCH_TRIPLE_TAP_R = 0x05;
constexpr uint8_t TOUCH_LONG_PRESS   = 0x17;
constexpr uint8_t TOUCH_RELEASE      = 0x18;

// Screen status
constexpr uint8_t SCREEN_TEXT       = 0x70;
constexpr uint8_t SCREEN_AI_AUTO    = 0x30;
constexpr uint8_t SCREEN_AI_COMPLETE = 0x40;
constexpr uint8_t SCREEN_AI_MANUAL  = 0x50;
constexpr uint8_t SCREEN_AI_ERROR   = 0x60;
constexpr uint8_t SCREEN_NEW_CONTENT = 0x01;

}  // namespace eveng1
