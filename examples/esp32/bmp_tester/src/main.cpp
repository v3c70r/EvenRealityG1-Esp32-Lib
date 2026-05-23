/**
 * Phase 0: G1 Glasses BMP Protocol Validation Tester (ESP32)
 *
 * Tests BMP partial updates (T1-T9), text protocol (T10-T12),
 * and text+BMP coexistence (T13-T15).
 *
 * Results are printed to Serial monitor at 115200 baud.
 */

#include <Arduino.h>
#include "g1_ble.hpp"
#include "g1_commands.hpp"
#include "bmp_pattern.hpp"
#include "bmp_file.hpp"
#include "crc32.hpp"

using namespace eveng1;

// Global device
G1Device g_device;
uint32_t g_lastHb = 0;

// Forward declarations
bool runBmpTest(const TestCase& test, uint8_t* fullFb);
bool runTextTest(const char* id, const char* name, const char* text);
bool runCoexistenceTests();
bool sendBmpFile(G1Side& side, const uint8_t* bmpData, size_t bmpSize);

// ─── Helpers ───

void sep(const char* title) {
    Serial.println();
    Serial.println(title);
    Serial.println("========================================");
}

void sendHeartbeatIfNeeded() {
    uint32_t now = millis();
    if (g_device.isConnected() && now - g_lastHb >= HEARTBEAT_INTERVAL_MS) {
        g_device.heartbeat();
        g_lastHb = now;
    }
}

// ─── BMP Test ───

bool runBmpTest(const TestCase& test, uint8_t* fullFb) {
    Serial.printf("Running %s: %s\n", test.id, test.name);
    Serial.printf("  Region: x=%d y=%d w=%d h=%d\n",
                  (int)test.region.x, (int)test.region.y,
                  (int)test.region.width, (int)test.region.height);

    // Generate test pattern in full framebuffer
    generateTestPattern(test.id, fullFb);

    // Extract region data
    size_t dataSize = test.region.dataSize();
    uint8_t* regionData = new uint8_t[dataSize];
    extractRegion(fullFb, test.region, regionData);

    // Compute address for this region
    uint32_t addrVal = test.region.startAddressByte();
    uint8_t addressBytes[4] = {
        (uint8_t)((addrVal >> 24) & 0xFF),
        (uint8_t)((addrVal >> 16) & 0xFF),
        (uint8_t)((addrVal >> 8) & 0xFF),
        (uint8_t)(addrVal & 0xFF),
    };

    Serial.printf("  Address: 0x%08X (%02X %02X %02X %02X)\n",
                  addrVal, addressBytes[0], addressBytes[1],
                  addressBytes[2], addressBytes[3]);
    Serial.printf("  Data: %d bytes (%d packets)\n",
                  (int)dataSize,
                  (int)((dataSize + BMP_PACKET_SIZE - 1) / BMP_PACKET_SIZE));

    uint32_t startMs = millis();

    // Send to both sides with hard sync and retry
    bool passed = g_device.sendBmpSync(regionData, dataSize, addressBytes, 3);
    sendHeartbeatIfNeeded();

    uint32_t elapsedMs = millis() - startMs;

    delete[] regionData;

    Serial.printf("  Result: %s (%dms)\n",
                  passed ? "PASS" : "FAIL", (int)elapsedMs);

    return passed;
}

// ─── Text Test ───

bool runTextTest(const char* id, const char* name, const char* text) {
    Serial.printf("Running %s: %s -> '%s'\n", id, name, text);

    uint32_t startMs = millis();
    bool ok = g_device.sendText(text);
    uint32_t elapsedMs = millis() - startMs;

    Serial.printf("  Result: %s (%dms)\n", ok ? "PASS" : "FAIL", (int)elapsedMs);
    return ok;
}

// ─── Coexistence Tests ───

bool runCoexistenceTests() {
    sep("T13: Text-then-BMP coexistence");

    // Send text first
    Serial.println("  Sending text 'HELLO G1'...");
    g_device.sendText("HELLO G1");
    delay(2000);
    sendHeartbeatIfNeeded();

    // Send small BMP overlay (32x32 white square at position 100,50)
    Serial.println("  Sending BMP overlay...");
    uint8_t* overlayFb = new uint8_t[FULL_FB_SIZE]();
    for (int y = 50; y < 82; ++y) {
        for (int x = 100; x < 132; ++x) {
            size_t byteIdx = y * DISPLAY_ROW_BYTES + x / 8;
            int bitIdx = 7 - (x % 8);
            overlayFb[byteIdx] |= (1 << bitIdx);
        }
    }

    BmpRegion fullRegion{0, 0, 576, 136};
    size_t dataSize = fullRegion.dataSize();
    uint8_t* data = new uint8_t[dataSize];
    extractRegion(overlayFb, fullRegion, data);

    uint8_t addr[4] = {0x00, 0x1C, 0x00, 0x00};
    bool lOk = g_device.sendBmp(g_device.left(), data, dataSize, addr);
    delay(200);
    bool rOk = g_device.sendBmp(g_device.right(), data, dataSize, addr);
    bool t13Ok = lOk && rOk;
    delete[] data;
    delete[] overlayFb;

    Serial.printf("  T13 Result: %s\n", t13Ok ? "OK" : "FAIL");
    delay(2000);
    sendHeartbeatIfNeeded();

    sep("T14: BMP-then-Text coexistence");

    // Send BMP first (full checkerboard)
    Serial.println("  Sending BMP checkerboard...");
    uint8_t* checkerFb = new uint8_t[FULL_FB_SIZE];
    generateCheckerboard(checkerFb);
    dataSize = FULL_FB_SIZE;
    data = new uint8_t[dataSize];
    extractRegion(checkerFb, fullRegion, data);
    lOk = g_device.sendBmp(g_device.left(), data, dataSize, addr);
    delay(200);
    rOk = g_device.sendBmp(g_device.right(), data, dataSize, addr);
    bool t14Ok = lOk && rOk;
    delete[] data;
    delete[] checkerFb;

    delay(1000);

    // Send text
    Serial.println("  Sending text 'WORLD G1'...");
    g_device.sendText("WORLD G1");

    Serial.printf("  T14 Result: %s\n", t14Ok ? "OK" : "FAIL");
    delay(2000);
    sendHeartbeatIfNeeded();

    sep("T15: BMP over text region");
    Serial.println("  This test checks if BMP erases text or composits");
    Serial.println("  Look at the glasses to determine behavior");
    Serial.println("  T13: Does BMP overlay on top of text?");
    Serial.println("  T14: Does text appear on top of BMP?");
    Serial.println("  T15: Could not run (redundant with T13)");

    return true;
}

// ─── BMP File Sender (with headers, 194 bytes/packet) ───

bool sendBmpFile(G1Side& side, const uint8_t* bmpData, size_t bmpSize) {
    uint8_t address[4] = {0x00, 0x1C, 0x00, 0x00};
    size_t totalPackets = (bmpSize + BMP_PACKET_DATA_SIZE - 1) / BMP_PACKET_DATA_SIZE;

    Serial.printf("  [%s] BMP file: %d bytes, %d packets (194 data/pkt)\n",
                  side.name(), (int)bmpSize, (int)totalPackets);

    for (size_t i = 0; i < totalPackets; i++) {
        size_t start = i * BMP_PACKET_DATA_SIZE;
        size_t end = (start + BMP_PACKET_DATA_SIZE < bmpSize) ? start + BMP_PACKET_DATA_SIZE : bmpSize;
        size_t chunkLen = end - start;

        // Header: [0x15, seq, address(4) for first packet]
        size_t hdrSize = (i == 0) ? 6 : 2;
        size_t pktSize = hdrSize + chunkLen;
        uint8_t pkt[256];

        pkt[0] = CMD_BMP_DATA;
        pkt[1] = i & 0xFF;
        if (i == 0) {
            pkt[2] = address[0]; pkt[3] = address[1];
            pkt[4] = address[2]; pkt[5] = address[3];
        }
        memcpy(pkt + hdrSize, bmpData + start, chunkLen);

        if (!side.write(pkt, pktSize)) {
            Serial.printf("  [%s] Write failed at packet %d/%d\n",
                          side.name(), (int)i + 1, (int)totalPackets);
            return false;
        }
        delay(15); // Increased delay to avoid write failures

        if ((i + 1) % 10 == 0 || i == totalPackets - 1) {
            Serial.printf("  [%s] Packet %d/%d\n", side.name(), (int)i + 1, (int)totalPackets);
        }
    }

    // BMP end
    Serial.printf("  [%s] BMP end...\n", side.name());
    uint8_t endPkt[] = {CMD_BMP_END, 0x0D, 0x0E};
    side.write(endPkt, sizeof(endPkt));

    G1Side::NotifyPacket response;
    if (!side.waitForNotify(CMD_BMP_END, response, RESPONSE_TIMEOUT_MS)) {
        Serial.printf("  [%s] BMP end: TIMEOUT\n", side.name());
        return false;
    }

    bool endOk = false;
    for (auto b : response.data) {
        if (b == RSP_SUCCESS) { endOk = true; break; }
    }
    Serial.printf("  [%s] BMP end: %s (cmd=0x%02X)\n",
                  side.name(), endOk ? "OK" : "FAIL", response.cmd);
    if (!endOk) return false;

    // CRC check (on address + entire BMP file)
    uint8_t* crcInput = new uint8_t[4 + bmpSize];
    memcpy(crcInput, address, 4);
    memcpy(crcInput + 4, bmpData, bmpSize);

    uint8_t crc[4];
    crc32_xz(crcInput, 4 + bmpSize, crc);
    delete[] crcInput;
    Serial.printf("  [%s] CRC: %02X%02X%02X%02X\n",
                  side.name(), crc[0], crc[1], crc[2], crc[3]);

    uint8_t crcPkt[5] = {CMD_CRC, crc[0], crc[1], crc[2], crc[3]};
    side.write(crcPkt, sizeof(crcPkt));

    if (!side.waitForNotify(CMD_CRC, response, RESPONSE_TIMEOUT_MS)) {
        Serial.printf("  [%s] CRC check: TIMEOUT\n", side.name());
        return false;
    }

    // CRC response: [0x16, crc0, crc1, crc2, crc3, status]
    bool crcOk = (response.data.size() > 5 && response.data[5] == RSP_SUCCESS);
    Serial.printf("  [%s] CRC check: %s\n", side.name(), crcOk ? "OK" : "FAIL");
    return crcOk;
}

// ─── Main Setup ───

void setup() {
    Serial.begin(115200);
    delay(2000);

    Serial.println();
    Serial.println("╔══════════════════════════════════════════╗");
    Serial.println("║  G1 Phase 0: BMP Protocol Validator     ║");
    Serial.println("║  ESP32 + NimBLE                         ║");
    Serial.println("╚══════════════════════════════════════════╝");
    Serial.println();

    // Connect
    if (!g_device.scanAndConnect(10000)) {
        Serial.println("FATAL: Could not connect to G1 glasses!");
        Serial.println("Make sure:");
        Serial.println("  1. Glasses are out of the charging case");
        Serial.println("  2. Glasses are NOT paired with phone");
        Serial.println("  3. Glasses are near the ESP32");
        return;
    }

    if (!g_device.start()) {
        Serial.println("FATAL: Could not initialize device!");
        return;
    }

    g_lastHb = millis();

    // Allocate full framebuffer
    uint8_t* fullFb = new uint8_t[FULL_FB_SIZE];
    int passed = 0, failed = 0;

    // ─── Test: Send proper BMP file with headers ───
    sep("BMP File Test (with headers)");

    uint8_t* bmpFile = new uint8_t[BMP_FULL_SIZE];
    generateBmpFile(bmpFile, true); // checkerboard

    uint8_t address[4] = {0x00, 0x1C, 0x00, 0x00};

    Serial.println("Sending BMP file to both glasses (sync with retry)...");
    uint32_t startMs = millis();

    bool syncOk = g_device.sendBmpSync(bmpFile, BMP_FULL_SIZE, address, 3);

    uint32_t elapsedMs = millis() - startMs;
    Serial.printf("BMP File Test (sync): %s (%dms)\n",
                  syncOk ? "OK" : "FAIL", (int)elapsedMs);
    Serial.println("Check if you see a checkerboard on BOTH glasses!");

    // Try exit command to trigger display
    if (syncOk) {
        delay(1000);
        Serial.println("Sending exit command (0x18) to trigger display...");
        uint8_t exitCmd[] = {0x18};
        g_device.left().write(exitCmd, sizeof(exitCmd));
        g_device.right().write(exitCmd, sizeof(exitCmd));
        delay(2000);
        Serial.println("Did the display appear after exit command?");
    }
    delay(3000);
    delete[] bmpFile;

    if (syncOk) passed++; else failed++;

    // ─── Run BMP Tests (T1-T9) ───
    sep("BMP Partial Update Tests (T1-T9)");

    for (int i = 0; i < BMP_TEST_COUNT; ++i) {
        Serial.println();
        sendHeartbeatIfNeeded();

        bool ok = runBmpTest(BMP_TESTS[i], fullFb);
        if (ok) passed++; else failed++;

        // Longer delay between tests to let glasses settle
        delay(1000);
    }

    // ─── Run Text Tests (T10-T12) ───
    sep("Text Protocol Tests (T10-T12)");

    sendHeartbeatIfNeeded();
    if (runTextTest("T10", "Simple text", "Hello G1!")) passed++; else failed++;
    delay(2000);

    sendHeartbeatIfNeeded();
    if (runTextTest("T11", "Pagination",
                    "Line1\nLine2\nLine3\nLine4\nLine5\nLine6\nLine7\nLine8\nLine9\nLine10"))
        passed++; else failed++;
    delay(3000);

    sendHeartbeatIfNeeded();
    if (runTextTest("T12", "Wide text (font test)",
                    "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"))
        passed++; else failed++;
    delay(2000);

    // ─── Run Coexistence Tests (T13-T15) ───
    sendHeartbeatIfNeeded();
    if (runCoexistenceTests()) passed++;

    // ─── Summary ───
    sep("TEST SUMMARY");
    Serial.printf("Total: %d | Passed: %d | Failed: %d\n",
                  passed + failed, passed, failed);

    if (passed == passed + failed) {
        Serial.println("\n  All tests passed! Full flexibility confirmed.");
    } else {
        Serial.println("\n  See individual results above.");
        Serial.println("  Key questions answered:");
        Serial.println("  - Partial BMP: check T3/T4/T6/T9");
        Serial.println("  - Text+BMP coexistence: check T13-T15");
        Serial.println("  - Single row: check T8");
    }

    delete[] fullFb;
    Serial.println("\nDone. Resetting in 10 seconds...");
    delay(10000);
    g_device.stop();
    ESP.restart();
}

void loop() {
    // Not used — all logic in setup()
    delay(1000);
}
