#include "g1_ble.hpp"
#include "g1_commands.hpp"
#include "crc32.hpp"
#include <Arduino.h>
#include <cstring>

namespace eveng1 {

// ─── G1Side ───

G1Side::~G1Side() { disconnect(); }

bool G1Side::connect(NimBLEAdvertisedDevice* device) {
    disconnect();
    _client = NimBLEDevice::createClient();
    if (!_client) {
        Serial.printf("  [%s] Failed to create client\n", _name);
        return false;
    }
    _client->setConnectionParams(24, 40, 0, 400);
    _client->setConnectTimeout(30000);

    Serial.printf("  [%s] Connect to %s (RSSI=%d)\n", _name,
                  device->getAddress().toString().c_str(), device->getRSSI());

    if (!_client->connect(device, false)) {
        int err = _client->getLastError();
        Serial.printf("  [%s] Connect failed err=0x%04X\n", _name, err);
        if (err == 517) Serial.println("  >>> Glasses bonded to another device");
        NimBLEDevice::deleteClient(_client);
        _client = nullptr;
        return false;
    }
    Serial.printf("  [%s] OK MTU=%d\n", _name, _client->getMTU());

    auto* svc = _client->getService(NUS_SERVICE_UUID);
    if (!svc) { Serial.printf("  [%s] NUS not found\n", _name); disconnect(); return false; }
    _txChar = svc->getCharacteristic(NUS_TX_CHAR_UUID);
    _rxChar = svc->getCharacteristic(NUS_RX_CHAR_UUID);
    if (!_txChar || !_rxChar) {
        Serial.printf("  [%s] NUS chars not found\n", _name); disconnect(); return false;
    }

    if (!_rxChar->subscribe(true, [this](NimBLERemoteCharacteristic*, uint8_t* d, size_t n, bool) {
            this->_handleNotify(d, n);
        })) {
        Serial.printf("  [%s] Notify subscribe failed\n", _name); disconnect(); return false;
    }
    Serial.printf("  [%s] NUS ready\n", _name);
    return true;
}

void G1Side::disconnect() {
    if (_client) {
        if (_client->isConnected()) _client->disconnect();
        NimBLEDevice::deleteClient(_client);
        _client = nullptr;
    }
    _txChar = nullptr; _rxChar = nullptr;
    portENTER_CRITICAL(&_queueMux);
    while (!_responseQueue.empty()) _responseQueue.pop();
    portEXIT_CRITICAL(&_queueMux);
}

bool G1Side::write(const uint8_t* data, uint16_t len, bool response) {
    if (!_txChar || !_client || !_client->isConnected()) return false;
    return _txChar->writeValue(data, len, false);
}

bool G1Side::waitForNotify(uint8_t expectedCmd, NotifyPacket& out, uint32_t timeoutMs) {
    uint32_t start = millis();
    while (millis() - start < timeoutMs) {
        portENTER_CRITICAL(&_queueMux);
        bool has = !_responseQueue.empty();
        if (has) { out = _responseQueue.front(); _responseQueue.pop(); }
        portEXIT_CRITICAL(&_queueMux);
        if (has) {
            if (out.cmd == expectedCmd) return true;
        }
        delay(10);
    }
    return false;
}

void G1Side::_handleNotify(const uint8_t* data, size_t length) {
    if (length == 0) return;
    uint8_t cmd = data[0];
    NotifyPacket pkt;
    pkt.cmd = cmd;
    pkt.data.assign(data, data + length);

    portENTER_CRITICAL(&_queueMux);
    _responseQueue.push(pkt);
    portEXIT_CRITICAL(&_queueMux);

    if (_notifyCb) _notifyCb(cmd, data + 1, length - 1);
}

// ─── G1Device ───

G1Device::~G1Device() { stop(); }

bool G1Device::scanAndConnect(uint32_t scanDurationMs) {
    NimBLEDevice::init("");
    NimBLEDevice::setPower(ESP_PWR_LVL_P9);
    NimBLEDevice::setSecurityAuth(true, true, true);
    NimBLEDevice::setSecurityIOCap(BLE_HS_IO_NO_INPUT_OUTPUT);
    delay(100);

    auto* scanner = NimBLEDevice::getScan();
    scanner->setActiveScan(true);
    scanner->setInterval(100);
    scanner->setWindow(99);

    Serial.println("Scanning...");
    auto results = scanner->getResults(scanDurationMs);

    NimBLEAdvertisedDevice* leftDev = nullptr;
    NimBLEAdvertisedDevice* rightDev = nullptr;
    int g1Count = 0;

    for (auto* dev : results) {
        if (!dev) continue;
        std::string name = dev->getName();
        if (name.find("G1") != std::string::npos) {
            g1Count++;
            Serial.printf("  %s (RSSI=%d)\n", name.c_str(), dev->getRSSI());
            if (name.find("_L_") != std::string::npos) leftDev = dev;
            else if (name.find("_R_") != std::string::npos) rightDev = dev;
        }
    }

    if (!leftDev || !rightDev) {
        Serial.printf("ERROR: L=%d R=%d\n", leftDev ? 1 : 0, rightDev ? 1 : 0);
        return false;
    }

    if (!_left.connect(leftDev)) return false;
    delay(200);
    if (!_right.connect(rightDev)) return false;

    // Register notification router
    _left.setNotifyCallback([this](uint8_t c, const uint8_t* d, uint16_t l) { _onNotification(c, d, l); });
    _right.setNotifyCallback([this](uint8_t c, const uint8_t* d, uint16_t l) { _onNotification(c, d, l); });

    return true;
}

bool G1Device::start() {
    Serial.println("Sending init...");

    // Primary display init — REQUIRED before any BMP/text
    uint8_t init[] = {CMD_INIT, 0x01};
    _left.write(init, sizeof(init));
    _right.write(init, sizeof(init));
    delay(500);

    // Disable wear detection so display stays visible
    uint8_t wear[] = {CMD_WEAR_DETECTION, 0x00};
    _left.write(wear, sizeof(wear));
    _right.write(wear, sizeof(wear));
    delay(100);

    Serial.println("Device ready");
    return true;
}

void G1Device::stop() {
    _left.disconnect();
    _right.disconnect();
}

void G1Device::heartbeat() {
    _hbSeq++;
    uint8_t hb[] = {CMD_HEARTBEAT, 0x06, 0x00, _hbSeq, 0x04, _hbSeq};
    _left.write(hb, sizeof(hb));
    _right.write(hb, sizeof(hb));
}

// ─── Display Settings ───

bool G1Device::setBrightness(uint8_t level, bool autoLight) {
    if (level > BRIGHTNESS_MAX) level = BRIGHTNESS_MAX;
    uint8_t pkt[] = {CMD_BRIGHTNESS, level, (uint8_t)(autoLight ? 1 : 0)};
    return _left.write(pkt, sizeof(pkt)) && _right.write(pkt, sizeof(pkt));
}

bool G1Device::setHeadUpAngle(uint8_t angle) {
    if (angle > HEAD_UP_ANGLE_MAX) angle = HEAD_UP_ANGLE_MAX;
    uint8_t pkt[] = {CMD_HEAD_UP_ANGLE, angle, 0x01};
    return _left.write(pkt, sizeof(pkt)) && _right.write(pkt, sizeof(pkt));
}

bool G1Device::setDashboardPosition(uint8_t height, uint8_t depth) {
    if (height > DASHBOARD_HEIGHT_MAX) height = DASHBOARD_HEIGHT_MAX;
    if (depth < DASHBOARD_DEPTH_MIN) depth = DASHBOARD_DEPTH_MIN;
    if (depth > DASHBOARD_DEPTH_MAX) depth = DASHBOARD_DEPTH_MAX;
    static uint8_t cnt = 0;
    uint8_t pkt[] = {CMD_DASHBOARD_POS, 0x08, 0x00, cnt++, 0x02, 0x01, height, depth};
    return _left.write(pkt, sizeof(pkt)) && _right.write(pkt, sizeof(pkt));
}

// ─── System ───

bool G1Device::queryBattery() {
    uint8_t pkt[] = {CMD_BATTERY_QUERY, 0x01};
    return _left.write(pkt, sizeof(pkt)) && _right.write(pkt, sizeof(pkt));
}

bool G1Device::setWearDetection(bool enabled) {
    uint8_t pkt[] = {CMD_WEAR_DETECTION, (uint8_t)(enabled ? 0x01 : 0x00)};
    return _left.write(pkt, sizeof(pkt)) && _right.write(pkt, sizeof(pkt));
}

bool G1Device::setSilentMode(bool enabled) {
    uint8_t pkt[] = {CMD_SILENT_MODE, (uint8_t)(enabled ? 0x01 : 0x0A)};
    return _left.write(pkt, sizeof(pkt)) && _right.write(pkt, sizeof(pkt));
}

bool G1Device::setMicEnabled(bool enabled) {
    uint8_t pkt[] = {CMD_MIC, (uint8_t)(enabled ? 1 : 0)};
    return _left.write(pkt, sizeof(pkt)) && _right.write(pkt, sizeof(pkt));
}

bool G1Device::sendExit() {
    uint8_t pkt[] = {CMD_EXIT};
    return _left.write(pkt, sizeof(pkt)) && _right.write(pkt, sizeof(pkt));
}

// ─── BMP / Text ───

bool G1Device::sendBmp(G1Side& side, const uint8_t* data, size_t dataLen,
                       const uint8_t address[4]) {
    // CRC first (reference app order)
    uint8_t crc[4];
    crc32_xz_bmp(BMP_ADDR_BYTES, data, dataLen, crc);
    uint8_t crcPkt[5] = {CMD_CRC, crc[0], crc[1], crc[2], crc[3]};
    side.write(crcPkt, sizeof(crcPkt));

    if (!_sendBmpData(side, data, dataLen, address)) {
        Serial.printf("  BMP data fail\n");
        return false;
    }

    uint8_t endPkt[] = {CMD_BMP_END, 0x0D, 0x0E};
    side.write(endPkt, sizeof(endPkt));

    G1Side::NotifyPacket resp;
    if (!side.waitForNotify(CMD_BMP_END, resp, RESPONSE_TIMEOUT_MS)) {
        Serial.printf("  BMP end TIMEOUT\n");
        return false;
    }
    for (auto b : resp.data) {
        if (b == RSP_SUCCESS) return true;
    }
    Serial.printf("  BMP end no ACK\n");
    return false;
}

bool G1Device::_sendBmpData(G1Side& side, const uint8_t* data, size_t dataLen,
                            const uint8_t address[4]) {
    uint32_t totalPackets = (dataLen + BMP_PACKET_SIZE - 1) / BMP_PACKET_SIZE;
    for (uint32_t i = 0; i < totalPackets; ++i) {
        uint32_t start = i * BMP_PACKET_SIZE;
        uint32_t end = (start + BMP_PACKET_SIZE < dataLen) ? start + BMP_PACKET_SIZE : dataLen;
        uint32_t chunkLen = end - start;

        size_t headerSize = (i == 0) ? 6 : 2;
        size_t pktSize = headerSize + chunkLen;
        uint8_t* pkt = new uint8_t[pktSize];

        pkt[0] = CMD_BMP_DATA;
        pkt[1] = i & 0xFF;
        if (i == 0) {
            pkt[2] = address[0]; pkt[3] = address[1];
            pkt[4] = address[2]; pkt[5] = address[3];
        }
        memcpy(pkt + headerSize, data + start, chunkLen);

        bool ok = side.write(pkt, pktSize);
        delete[] pkt;
        if (!ok) return false;
        delay(15);
    }
    return true;
}

bool G1Device::sendBmpSync(const uint8_t* data, size_t dataLen,
                            const uint8_t address[4], int maxRetries) {
    for (int attempt = 0; attempt < maxRetries; attempt++) {
        if (attempt > 0) { delay(500); }
        Serial.printf("BMP attempt %d/%d... ", attempt + 1, maxRetries);
        bool lOk = sendBmp(_left, data, dataLen, address);
        delay(200);
        bool rOk = sendBmp(_right, data, dataLen, address);
        Serial.printf("L=%d R=%d\n", lOk, rOk);
        if (lOk && rOk) return true;
        if (attempt < maxRetries - 1) {
            heartbeat();
            delay(1000);
        }
    }
    return false;
}

bool G1Device::sendText(const char* text) {
    std::string textStr(text);
    size_t len = textStr.size();
    constexpr size_t MAX_CHUNK = 191;
    uint32_t totalPackets = (len + MAX_CHUNK - 1) / MAX_CHUNK;

    for (auto* side : {&_left, &_right}) {
        for (uint32_t i = 0; i < totalPackets; ++i) {
            size_t start = i * MAX_CHUNK;
            size_t chunkLen = (start + MAX_CHUNK < len) ? MAX_CHUNK : len - start;
            uint8_t pkt[256];
            pkt[0] = CMD_TEXT;
            pkt[1] = i & 0xFF;
            pkt[2] = totalPackets & 0xFF;
            pkt[3] = i & 0xFF;
            pkt[4] = SCREEN_TEXT;
            pkt[5] = 0x00; pkt[6] = 0x00;
            pkt[7] = 0x00; pkt[8] = 0x01;
            memcpy(pkt + 9, textStr.c_str() + start, chunkLen);
            side->write(pkt, 9 + chunkLen);
            delay(10);
        }
        delay(100);
    }
    return true;
}

// ─── Event Handler ───

void G1Device::_onNotification(uint8_t cmd, const uint8_t* data, uint16_t len) {
    if (cmd != CMD_TOUCHBAR && cmd != CMD_BATTERY_QUERY) return;

    if (cmd == CMD_BATTERY_QUERY && len >= 2 && data[0] == 0x66) {
        int batt = data[1];
        _batteryLeft = batt;
        _batteryRight = batt;  // Both sides report same battery
        snprintf(_lastEvent, sizeof(_lastEvent), "BAT:%d%%", batt);
        if (_eventCb) _eventCb(G1Event::BATTERY_UPDATE, data, len);
        return;
    }

    if (cmd == CMD_TOUCHBAR && len >= 1) {
        uint8_t evt = data[0];
        switch (evt) {
            case TOUCH_SINGLE_TAP:
                snprintf(_lastEvent, sizeof(_lastEvent), "TAP SINGLE");
                if (_eventCb) _eventCb(G1Event::SINGLE_TAP_RIGHT, data, len);
                break;
            case TOUCH_DOUBLE_TAP:
                snprintf(_lastEvent, sizeof(_lastEvent), "TAP DOUBLE");
                if (_eventCb) _eventCb(G1Event::DOUBLE_TAP_RIGHT, data, len);
                break;
            case TOUCH_TRIPLE_TAP_L:
                snprintf(_lastEvent, sizeof(_lastEvent), "TAP TRIPLE");
                if (_eventCb) _eventCb(G1Event::TRIPLE_TAP_LEFT, data, len);
                break;
            case TOUCH_LONG_PRESS:
                snprintf(_lastEvent, sizeof(_lastEvent), "LONG PRESS");
                if (_eventCb) _eventCb(G1Event::LONG_PRESS_LEFT, data, len);
                break;
            case EVT_HEAD_UP:
                snprintf(_lastEvent, sizeof(_lastEvent), "HEAD UP");
                if (_eventCb) _eventCb(G1Event::HEAD_UP, data, len);
                break;
            case EVT_HEAD_DOWN:
                snprintf(_lastEvent, sizeof(_lastEvent), "HEAD DOWN");
                if (_eventCb) _eventCb(G1Event::HEAD_DOWN, data, len);
                break;
            case EVT_CASE_REMOVED:
                snprintf(_lastEvent, sizeof(_lastEvent), "OFF FACE");
                if (_eventCb) _eventCb(G1Event::CASE_REMOVED, data, len);
                break;
            case EVT_CASE_OPEN:
                snprintf(_lastEvent, sizeof(_lastEvent), "CASE OPEN");
                if (_eventCb) _eventCb(G1Event::CASE_OPEN, data, len);
                break;
            case EVT_CASE_CLOSED:
                snprintf(_lastEvent, sizeof(_lastEvent), "CASE CLOSED");
                if (_eventCb) _eventCb(G1Event::CASE_CLOSED, data, len);
                break;
            case EVT_CASE_CHARGING:
                snprintf(_lastEvent, sizeof(_lastEvent), "CHARGING");
                if (_eventCb) _eventCb(G1Event::CASE_CHARGING, data, len);
                break;
            default:
                snprintf(_lastEvent, sizeof(_lastEvent), "EVT:0x%02X", evt);
                break;
        }
    }
}

const char* G1Device::_eventName(G1Event e) {
    switch (e) {
        case G1Event::SINGLE_TAP_LEFT:  return "TAP L";
        case G1Event::SINGLE_TAP_RIGHT: return "TAP R";
        case G1Event::DOUBLE_TAP_LEFT:  return "2TAP L";
        case G1Event::DOUBLE_TAP_RIGHT: return "2TAP R";
        case G1Event::TRIPLE_TAP_LEFT:  return "3TAP L";
        case G1Event::TRIPLE_TAP_RIGHT: return "3TAP R";
        case G1Event::LONG_PRESS_LEFT:  return "HOLD L";
        case G1Event::LONG_PRESS_RIGHT: return "HOLD R";
        case G1Event::HEAD_UP:          return "HEAD UP";
        case G1Event::HEAD_DOWN:        return "HEAD DN";
        case G1Event::CASE_REMOVED:     return "CASE OUT";
        case G1Event::CASE_OPEN:        return "CASE OPEN";
        case G1Event::CASE_CLOSED:      return "CASE CLOSE";
        case G1Event::CASE_CHARGING:    return "CHARGING";
        case G1Event::BATTERY_UPDATE:   return "BATTERY";
        default: return "?";
    }
}

}  // namespace eveng1
