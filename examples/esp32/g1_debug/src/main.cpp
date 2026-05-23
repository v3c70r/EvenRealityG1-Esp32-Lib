/**
 * G1 Debug Tool — Interactive ESP32 debugger for Even G1 glasses.
 *
 * Keys on Serial (115200 baud):
 *   Settings:  b/B=br+- a/A=angle+- h/H=dashH+- d/D=dashD+-
 *              w=toggle-wear s=toggle-silent m=toggle-mic
 *   Presets:   1=dim 2=med 3=high 4=max 5=daylight  0=auto
 *   Display:   t=text c=checkerboard i=info-on-glasses
 *   System:    v=query-battery x=exit r=reset ?=help
 *
 * Glasses lenses show live status bars using text protocol.
 * Events (touch, head, case) appear on Serial and lenses.
 */

#include <Arduino.h>
#include "g1_ble.hpp"
#include "g1_commands.hpp"
#include "bmp_pattern.hpp"
#include "crc32.hpp"

using namespace eveng1;

G1Device g_device;

// ─── State ───
struct {
    uint8_t  brightness    = 30;   // 0-63
    bool     autoLight     = false;
    uint8_t  headAngle     = 25;   // 0-60
    uint8_t  dashHeight    = 4;    // 0-8
    uint8_t  dashDepth     = 5;    // 1-9
    bool     wearEnabled   = false;
    bool     silentEnabled = false;
    bool     micEnabled    = false;
    char     lastEvent[48] = "—";
    uint32_t lastEventMs   = 0;
    int      battLevel     = -1;
} g_state;

uint32_t g_lastHb = 0, g_lastBatt = 0, g_lastDisplay = 0, g_bmpActiveUntil = 0;
uint8_t  g_fullFb[FULL_FB_SIZE];

// ─── Event Handler ───
void onGlassesEvent(G1Event event, const uint8_t* data, uint16_t len) {
    const char* names[] = {
        "SINGLE TAP L","SINGLE TAP R","DOUBLE TAP L","DOUBLE TAP R",
        "TRIPLE TAP L","TRIPLE TAP R","LONG HOLD L","LONG HOLD R",
        "HEAD UP","HEAD DOWN","OFF FACE","CASE OPEN",
        "CASE CLOSE","CHARGING","BATTERY"
    };
    int idx = (int)event;
    if (idx >= 0 && idx < 15) {
        snprintf(g_state.lastEvent, sizeof(g_state.lastEvent), "%s", names[idx]);
    }
    g_state.lastEventMs = millis();
    Serial.printf("[EVENT] %s\n", g_state.lastEvent);
}

void showHelp() {
    Serial.println();
    Serial.println(F("╔══════════════════════════════════════════╗"));
    Serial.println(F("║  G1 Debug Tool   Keys (Serial 115200)   ║"));
    Serial.println(F("╠══════════════════════════════════════════╣"));
    Serial.println(F("║ SETTINGS                                ║"));
    Serial.println(F("║  b/B  brightness -/+      (0-63)       ║"));
    Serial.println(F("║  a/A  head angle -/+      (0-60°)      ║"));
    Serial.println(F("║  h/H  dash height -/+     (0-8)        ║"));
    Serial.println(F("║  d/D  dash depth  -/+     (1-9)        ║"));
    Serial.println(F("║  w    toggle wear detection             ║"));
    Serial.println(F("║  s    toggle silent mode               ║"));
    Serial.println(F("║  m    toggle microphone                ║"));
    Serial.println(F("╠══════════════════════════════════════════╣"));
    Serial.println(F("║ PRESETS                                 ║"));
    Serial.println(F("║  1=10% 2=25% 3=50% 4=75% 5=100%        ║"));
    Serial.println(F("║  0    toggle auto brightness           ║"));
    Serial.println(F("╠══════════════════════════════════════════╣"));
    Serial.println(F("║ DISPLAY                                 ║"));
    Serial.println(F("║  t    send 'Hello G1!' text            ║"));
    Serial.println(F("║  c    checkerboard BMP                 ║"));
    Serial.println(F("║  i    show system info on glasses      ║"));
    Serial.println(F("║  p    draw status bars on glasses      ║"));
    Serial.println(F("╠══════════════════════════════════════════╣"));
    Serial.println(F("║ SYSTEM                                  ║"));
    Serial.println(F("║  S    show full status                  ║"));
    Serial.println(F("║  o    screen OFF (go home)             ║"));
    Serial.println(F("║  O    screen ON (wake)                 ║"));
    Serial.println(F("║  v    query battery                    ║"));
    Serial.println(F("║  x    send exit command                ║"));
    Serial.println(F("║  r    reset state                      ║"));
    Serial.println(F("║  ?    help                             ║"));
    Serial.println(F("╚══════════════════════════════════════════╝"));
    Serial.println();
}

void showStatus() {
    Serial.println();
    Serial.println(F("═══ STATUS ═══════════════════════════════"));
    Serial.printf("  Battery:      %d%%\n", g_state.battLevel);
    Serial.printf("  Brightness:   %d/63  %s\n",
                  g_state.brightness,
                  g_state.autoLight ? "(AUTO)" : "");
    Serial.printf("  Head Angle:   %d°\n", g_state.headAngle);
    Serial.printf("  Dash:         H=%d D=%d\n", g_state.dashHeight, g_state.dashDepth);
    Serial.printf("  Wear Detect:  %s\n", g_state.wearEnabled ? "ON" : "OFF");
    Serial.printf("  Silent Mode:  %s\n", g_state.silentEnabled ? "ON" : "OFF");
    Serial.printf("  Mic:          %s\n", g_state.micEnabled ? "ON" : "OFF");
    Serial.printf("  Last Event:   %s\n", g_state.lastEvent);
    Serial.println(F("═══════════════════════════════════════════"));
    Serial.println();
}

// ─── Display on glasses (text protocol) ───

void drawInfoOnGlasses() {
    if (!g_device.isConnected()) return;

    char buf[256];
    snprintf(buf, sizeof(buf),
             "G1 DEBUG TOOL\n"
             "BAT:%d%% BR:%d %s\n"
             "ANG:%d  H:%d D:%d\n"
             "Last: %s",
             g_state.battLevel,
             g_state.brightness, g_state.autoLight ? "AUTO" : "MAN",
             g_state.headAngle, g_state.dashHeight, g_state.dashDepth,
             g_state.lastEvent);
    g_device.sendText(buf);
}

void flashEventOnGlasses(const char* eventText) {
    if (!g_device.isConnected()) return;
    char buf[256];
    snprintf(buf, sizeof(buf), ">>> %s <<<", eventText);
    g_device.sendText(buf);
}

// ─── BMP Patterns ───

void drawCheckerboard() {
    memset(g_fullFb, 0, FULL_FB_SIZE);
    generateCheckerboard(g_fullFb);
    uint8_t addr[4] = {0x00, 0x1C, 0x00, 0x00};
    Serial.println("Sending checkerboard...");
    bool ok = g_device.sendBmpSync(g_fullFb, FULL_FB_SIZE, addr, 3);
    Serial.println(ok ? "Checkerboard OK" : "Checkerboard FAILED");
    g_bmpActiveUntil = millis() + 60000;
}

void drawStatusBars() {
    memset(g_fullFb, 0, FULL_FB_SIZE);

    // Battery bar at top: full width, 16px tall
    int batPct = (g_state.battLevel > 0) ? g_state.battLevel : 50;
    int batW = (batPct * 576) / 100;
    for (int y = 0; y < 16; ++y) {
        for (int x = 0; x < batW; ++x) {
            size_t bi = y * DISPLAY_ROW_BYTES + x / 8;
            g_fullFb[bi] |= (1 << (7 - (x % 8)));
        }
    }

    // Brightness bar: full width, 12px tall at y=30
    int brW = (g_state.brightness * 576) / BRIGHTNESS_MAX;
    for (int y = 30; y < 42; ++y) {
        for (int x = 0; x < brW; ++x) {
            size_t bi = y * DISPLAY_ROW_BYTES + x / 8;
            g_fullFb[bi] |= (1 << (7 - (x % 8)));
        }
    }

    // Head angle bar: at y=55, narrower for visual distinction
    int angW = (g_state.headAngle * 400) / HEAD_UP_ANGLE_MAX + 176;
    for (int y = 55; y < 65; ++y) {
        for (int x = 0; x < angW; ++x) {
            size_t bi = y * DISPLAY_ROW_BYTES + x / 8;
            g_fullFb[bi] |= (1 << (7 - (x % 8)));
        }
    }

    // Dash indicator: H bar at y=80, D bar at y=100
    int hW = ((g_state.dashHeight + 1) * 576) / (DASHBOARD_HEIGHT_MAX + 1);
    for (int y = 80; y < 92; ++y) {
        for (int x = 0; x < hW; ++x) {
            size_t bi = y * DISPLAY_ROW_BYTES + x / 8;
            g_fullFb[bi] |= (1 << (7 - (x % 8)));
        }
    }

    int dW = (g_state.dashDepth * 576) / (DASHBOARD_DEPTH_MAX + 1);
    for (int y = 100; y < 112; ++y) {
        for (int x = 0; x < dW; ++x) {
            size_t bi = y * DISPLAY_ROW_BYTES + x / 8;
            g_fullFb[bi] |= (1 << (7 - (x % 8)));
        }
    }

    uint8_t addr[4] = {0x00, 0x1C, 0x00, 0x00};
    bool ok = g_device.sendBmpSync(g_fullFb, FULL_FB_SIZE, addr, 3);
    Serial.println(ok ? "Status bars OK" : "Status bars FAILED");
    g_bmpActiveUntil = millis() + 60000;
}

// ─── Command Handler ───

void applyAllSettings() {
    g_device.setBrightness(g_state.brightness, g_state.autoLight);
    delay(30);
    g_device.setHeadUpAngle(g_state.headAngle);
    delay(30);
    g_device.setDashboardPosition(g_state.dashHeight, g_state.dashDepth);
    delay(30);
    g_device.setWearDetection(g_state.wearEnabled);
    delay(30);
    g_device.setSilentMode(g_state.silentEnabled);
    delay(30);
    g_device.setMicEnabled(g_state.micEnabled);
}

void handleCommand(char c) {
    Serial.printf("[CMD] %c\n", c);

    switch (c) {
        case 'b': if (g_state.brightness > 0) g_state.brightness--; g_device.setBrightness(g_state.brightness, g_state.autoLight); break;
        case 'B': if (g_state.brightness < BRIGHTNESS_MAX) g_state.brightness++; g_device.setBrightness(g_state.brightness, g_state.autoLight); break;
        case 'a': if (g_state.headAngle > 0) g_state.headAngle--; g_device.setHeadUpAngle(g_state.headAngle); break;
        case 'A': if (g_state.headAngle < HEAD_UP_ANGLE_MAX) g_state.headAngle++; g_device.setHeadUpAngle(g_state.headAngle); break;
        case 'h': if (g_state.dashHeight > 0) g_state.dashHeight--; g_device.setDashboardPosition(g_state.dashHeight, g_state.dashDepth); break;
        case 'H': if (g_state.dashHeight < DASHBOARD_HEIGHT_MAX) g_state.dashHeight++; g_device.setDashboardPosition(g_state.dashHeight, g_state.dashDepth); break;
        case 'd': if (g_state.dashDepth > DASHBOARD_DEPTH_MIN) g_state.dashDepth--; g_device.setDashboardPosition(g_state.dashHeight, g_state.dashDepth); break;
        case 'D': if (g_state.dashDepth < DASHBOARD_DEPTH_MAX) g_state.dashDepth++; g_device.setDashboardPosition(g_state.dashHeight, g_state.dashDepth); break;

        case 'w': g_state.wearEnabled = !g_state.wearEnabled; g_device.setWearDetection(g_state.wearEnabled); break;
        case 's': g_state.silentEnabled = !g_state.silentEnabled; g_device.setSilentMode(g_state.silentEnabled); break;
        case 'm': g_state.micEnabled = !g_state.micEnabled; g_device.setMicEnabled(g_state.micEnabled); break;

        case '0': g_state.autoLight = !g_state.autoLight; g_device.setBrightness(g_state.brightness, g_state.autoLight); break;
        case '1': g_state.brightness = 6;  g_state.autoLight = false; g_device.setBrightness(6, false); break;
        case '2': g_state.brightness = 16; g_state.autoLight = false; g_device.setBrightness(16, false); break;
        case '3': g_state.brightness = 32; g_state.autoLight = false; g_device.setBrightness(32, false); break;
        case '4': g_state.brightness = 48; g_state.autoLight = false; g_device.setBrightness(48, false); break;
        case '5': g_state.brightness = 63; g_state.autoLight = false; g_device.setBrightness(63, false); break;


        case 'c': Serial.println("Drawing checkerboard..."); drawCheckerboard(); break;
        case 'o': g_device.sendExit(); Serial.println("Screen OFF (home)"); break;

        case 'v': g_device.queryBattery(); break;
        case 'x': g_device.sendExit(); break;
        case 't': g_bmpActiveUntil = 0; g_device.sendText("Hello G1!\nDebug Tool\nESP32"); break;
        case 'O': g_bmpActiveUntil = 0; g_device.sendText("G1 Debug\nReady"); Serial.println("Screen ON (wake)"); break;
        case 'i': g_bmpActiveUntil = 0; drawInfoOnGlasses(); break;
        case 'p': Serial.println("Drawing status bars..."); drawStatusBars(); break;
        case '?': showHelp(); break;
        case 'S': showStatus(); break;
        case 'r':
            applyAllSettings();
            showStatus();
            break;
        default: break;
    }

    // Print setting change feedback
    if (c == 'b' || c == 'B') Serial.printf("  Brightness: %d/63\n", g_state.brightness);
    if (c == 'a' || c == 'A') Serial.printf("  Head Angle: %d\n", g_state.headAngle);
    if (c == 'h' || c == 'H' || c == 'd' || c == 'D') Serial.printf("  Dash: H=%d D=%d\n", g_state.dashHeight, g_state.dashDepth);
    if (c == 'w') Serial.printf("  Wear Detection: %s\n", g_state.wearEnabled ? "ON" : "OFF");
    if (c == 's') Serial.printf("  Silent Mode: %s\n", g_state.silentEnabled ? "ON" : "OFF");
    if (c == 'm') Serial.printf("  Mic: %s\n", g_state.micEnabled ? "ON" : "OFF");
    if (c == '0') Serial.printf("  Auto Light: %s\n", g_state.autoLight ? "ON" : "OFF");
}

// ─── Setup ───

void setup() {
    Serial.begin(115200);
    delay(2000);

    Serial.println(F("\n╔══════════════════════════════════════════╗"));
    Serial.println(F("║  G1 Debug Tool – Even G1 glasses       ║"));
    Serial.println(F("║  ESP32 + NimBLE                        ║"));
    Serial.println(F("╚══════════════════════════════════════════╝"));

    if (!g_device.scanAndConnect(10000)) {
        Serial.println(F("FATAL: Could not connect to G1 glasses!"));
        Serial.println(F("Check: 1) Out of case  2) Not paired with phone  3) Near ESP32"));
        return;
    }

    if (!g_device.start()) {
        Serial.println(F("FATAL: Init failed"));
        return;
    }

    // Register event handler
    g_device.setEventCallback(onGlassesEvent);

    // Apply defaults
    applyAllSettings();
    delay(200);

    // Query battery
    g_device.queryBattery();
    delay(200);

    g_lastHb = millis();
    g_lastBatt = millis();

    showHelp();

    // Warmup: heartbeat + short delay before first BMP
    g_device.heartbeat();
    delay(2000);
}

// ─── Loop ───

void loop() {
    uint32_t now = millis();

    // Heartbeat every 8s
    if (now - g_lastHb >= 8000) {
        g_device.heartbeat();
        g_lastHb = now;
    }

    // Battery query every 30s
    if (now - g_lastBatt >= 30000) {
        g_device.queryBattery();
        g_lastBatt = now;
        delay(100);
        g_state.battLevel = g_device.getLeftBattery();
    }

    // Auto-refresh glasses display every 4s (skip if BMP is active)
    if (now - g_lastDisplay >= 4000 && now - g_state.lastEventMs > 5000
        && now > g_bmpActiveUntil) {
        drawInfoOnGlasses();
        g_lastDisplay = now;
    }

    // Check serial input
    if (Serial.available()) {
        char c = Serial.read();
        if (c == '\n' || c == '\r') return;
        handleCommand(c);
        delay(50); // debounce
    }

    // Check for head movement events from glasses (poll)
    G1Side::NotifyPacket pkt;
    if (g_device.left().waitForNotify(0xFF, pkt, 0)) {
        // Quick non-blocking poll - handled by callback already
    }

    delay(10);
}
