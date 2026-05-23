/**
 * Minimal G1 Example — EvenG1Lib
 *
 * Connects to G1 glasses, displays "Hello World!" as text,
 * then draws a checkerboard BMP. Handles touch events.
 *
 * Hardware: ESP32-S3 + Even Reality G1 glasses
 * Serial: 115200 baud
 */

#include <Arduino.h>
#include "g1_ble.hpp"
#include "g1_commands.hpp"
#include "bmp_pattern.hpp"

using namespace eveng1;

G1Device g_device;
uint32_t g_lastHb = 0;

// Full framebuffer (heap allocation to avoid stack overflow)
uint8_t* g_fb = nullptr;

// ─── Event handler ───
void onEvent(uint8_t cmd, const uint8_t* data, uint16_t len) {
    if (cmd == CMD_TOUCHBAR && len >= 1) {
        switch (data[0]) {
            case 0x01: Serial.println("[TOUCH] Single tap"); break;
            case 0x00: Serial.println("[TOUCH] Double tap"); break;
            case 0x17: Serial.println("[TOUCH] Long press"); break;
            default:   Serial.printf("[TOUCH] 0x%02X\n", data[0]); break;
        }
    }
}

// ─── Setup ───
void setup() {
    Serial.begin(115200);
    delay(2000);
    Serial.println("\n=== EvenG1Lib Minimal Example ===\n");

    // Connect
    if (!g_device.scanAndConnect(10000)) {
        Serial.println("Failed to connect. Check:");
        Serial.println("  1. Glasses out of charging case");
        Serial.println("  2. Glasses NOT paired with phone");
        Serial.println("  3. ESP32 near glasses");
        return;
    }

    if (!g_device.start()) {
        Serial.println("Init failed");
        return;
    }

    // Register touch events
    g_device.left().setNotifyCallback(onEvent);
    g_device.right().setNotifyCallback(onEvent);

    g_lastHb = millis();

    // Send text to glasses
    Serial.println("Sending text...");
    g_device.sendText("Hello World!\nEvenG1Lib");
    delay(3000);

    // Draw checkerboard
    Serial.println("Drawing checkerboard...");
    g_fb = new uint8_t[FULL_FB_SIZE];
    memset(g_fb, 0, FULL_FB_SIZE);
    generateCheckerboard(g_fb);

    uint8_t addr[4] = {0x00, 0x1C, 0x00, 0x00};
    if (g_device.sendBmpSync(g_fb, FULL_FB_SIZE, addr, 3)) {
        Serial.println("Checkerboard OK");
    } else {
        Serial.println("Checkerboard FAILED");
    }

    // Exit to home screen to trigger display
    g_device.sendExit();
    Serial.println("Look at glasses — should show checkerboard");
}

// ─── Loop ───
void loop() {
    if (millis() - g_lastHb >= 8000) {
        g_device.heartbeat();
        g_lastHb = millis();
        Serial.print(".");
    }
    delay(1000);
}
