#include "g1_ble.hpp"
#include "g1_commands.hpp"
#include "crc32.hpp"
#include <Arduino.h>

namespace eveng1 {

// ─── G1Side ───

G1Side::~G1Side() {
    disconnect();
}

bool G1Side::connect(NimBLEAdvertisedDevice* device) {
    disconnect();

    _client = NimBLEDevice::createClient();
    if (!_client) {
        Serial.printf("  [%s] Failed to create client!\n", _name);
        return false;
    }

    _client->setConnectionParams(24, 40, 0, 400);
    _client->setConnectTimeout(30000);

    Serial.printf("  [%s] Connecting to %s (type=%d RSSI=%d)...\n", _name,
                  device->getAddress().toString().c_str(),
                  device->getAddress().getType(),
                  device->getRSSI());

    bool ok = _client->connect(device, false);
    if (!ok) {
        int err = _client->getLastError();
        Serial.printf("  [%s] Connect failed! err=%d (0x%04X)\n", _name, err, err);
        // 517 = HCI Auth Fail (glasses bonded elsewhere)
        // 13 = Timeout (glasses not responding)
        // 574 = HCI Conn Establishment Fail
        if (err == 517) {
            Serial.println("  >>> Glasses are bonded to another device!");
            Serial.println("  >>> Unpair from phone AND reset glasses (case + close temples)");
        }
        NimBLEDevice::deleteClient(_client);
        _client = nullptr;
        return false;
    }
    Serial.printf("  [%s] Connected (MTU=%d)\n", _name, _client->getMTU());

    // Discover UART service
    auto* svc = _client->getService(NUS_SERVICE_UUID);
    if (!svc) {
        Serial.printf("  [%s] NUS service not found!\n", _name);
        disconnect();
        return false;
    }

    _txChar = svc->getCharacteristic(NUS_TX_CHAR_UUID);
    _rxChar = svc->getCharacteristic(NUS_RX_CHAR_UUID);

    if (!_txChar || !_rxChar) {
        Serial.printf("  [%s] NUS chars not found!\n", _name);
        disconnect();
        return false;
    }

    // Enable notifications — NimBLE v2.x uses std::function callback
    if (!_rxChar->subscribe(true, [this](NimBLERemoteCharacteristic* pChar,
                                          uint8_t* pData, size_t length,
                                          bool isNotify) {
            this->_handleNotify(pData, length);
        })) {
        Serial.printf("  [%s] Subscribe failed!\n", _name);
        disconnect();
        return false;
    }

    Serial.printf("  [%s] Services OK, notify enabled\n", _name);
    return true;
}

void G1Side::disconnect() {
    if (_client) {
        if (_client->isConnected()) {
            _client->disconnect();
        }
        NimBLEDevice::deleteClient(_client);
        _client = nullptr;
    }
    _txChar = nullptr;
    _rxChar = nullptr;

    // Clear response queue
    portENTER_CRITICAL(&_queueMux);
    while (!_responseQueue.empty()) _responseQueue.pop();
    portEXIT_CRITICAL(&_queueMux);
}

bool G1Side::write(const uint8_t* data, uint16_t len, bool response) {
    if (!_txChar || !_client || !_client->isConnected()) {
        return false;
    }
    return _txChar->writeValue(data, len, false); // Always write without response like reference app
}

bool G1Side::waitForNotify(uint8_t expectedCmd, NotifyPacket& out, uint32_t timeoutMs) {
    uint32_t start = millis();
    while (millis() - start < timeoutMs) {
        portENTER_CRITICAL(&_queueMux);
        bool hasData = !_responseQueue.empty();
        if (hasData) {
            out = _responseQueue.front();
            _responseQueue.pop();
        }
        portEXIT_CRITICAL(&_queueMux);

        if (hasData) {
            if (out.cmd == expectedCmd) {
                return true;
            }
            // Not the response we want — discard and keep waiting
        }
        delay(10);
    }
    return false;
}

void G1Side::_notifyHandler(NimBLERemoteCharacteristic* pChar,
                             uint8_t* pData, size_t length,
                             bool isNotify) {
    // Legacy — kept for potential compatibility
}

void G1Side::_handleNotify(const uint8_t* data, size_t length) {
    if (length == 0) return;

    uint8_t cmd = data[0];

    // Route to response queue for waitForNotify
    NotifyPacket* pkt = new NotifyPacket();
    pkt->cmd = cmd;
    pkt->data.assign(data, data + length);

    portENTER_CRITICAL(&_queueMux);
    _responseQueue.push(std::move(*pkt));
    portEXIT_CRITICAL(&_queueMux);
    delete pkt;

    // Also route to user callback
    if (_notifyCb) {
        _notifyCb(cmd, data + 1, length - 1);
    }
}

// ─── G1Device ───

G1Device::~G1Device() {
    stop();
}

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

    Serial.println("Scanning for G1 devices...");
    auto results = scanner->getResults(scanDurationMs);

    NimBLEAdvertisedDevice* leftDev = nullptr;
    NimBLEAdvertisedDevice* rightDev = nullptr;
    int g1Count = 0;

    for (auto* dev : results) {
        if (!dev) continue;
        std::string name = dev->getName();
        if (name.find("G1") != std::string::npos) {
            g1Count++;
            Serial.printf("  G1 found: %s (%s) RSSI=%d type=%d\n",
                          name.c_str(), dev->getAddress().toString().c_str(),
                          dev->getRSSI(), dev->getAddress().getType());

            if (name.find("_L_") != std::string::npos) {
                leftDev = dev;
            } else if (name.find("_R_") != std::string::npos) {
                rightDev = dev;
            }
        }
    }

    Serial.printf("Found %d total, %d G1 devices\n", results.getCount(), g1Count);

    if (!leftDev || !rightDev) {
        Serial.printf("ERROR: Need both L and R! (L=%d R=%d)\n",
                      leftDev ? 1 : 0, rightDev ? 1 : 0);
        return false;
    }

    if (!_left.connect(leftDev)) return false;
    delay(500);
    if (!_right.connect(rightDev)) return false;

    return true;
}

bool G1Device::start() {
    Serial.println("Sending init...");
    uint8_t init[] = {CMD_INIT, 0x01};
    _left.write(init, sizeof(init));
    _right.write(init, sizeof(init));
    delay(500);

    Serial.println("G1 device ready");
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
    Serial.printf("HB seq=%d\n", _hbSeq);
}

bool G1Device::sendBmp(G1Side& side, const uint8_t* data, size_t dataLen,
                       const uint8_t address[4]) {
    // Step 1: Send BMP packets
    if (!_sendBmpData(side, data, dataLen, address)) {
        Serial.printf("  [%s] BMP data send failed\n", side.name());
        return false;
    }

    // Step 2: Send end packet
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
                  side.name(), endOk ? "OK" : "FAIL",
                  response.cmd);

    if (!endOk) return false;

    // Step 3: Send CRC check (always use BASE address for CRC!)
    uint8_t crc[4];
    crc32_xz_bmp(BMP_ADDR_BYTES, data, dataLen, crc);
    Serial.printf("  [%s] CRC: %02X%02X%02X%02X\n",
                  side.name(), crc[0], crc[1], crc[2], crc[3]);

    uint8_t crcPkt[5] = {CMD_CRC, crc[0], crc[1], crc[2], crc[3]};
    side.write(crcPkt, sizeof(crcPkt));

    if (!side.waitForNotify(CMD_CRC, response, RESPONSE_TIMEOUT_MS)) {
        Serial.printf("  [%s] CRC check: TIMEOUT\n", side.name());
        return false;
    }

    // Check for success (0xC9 somewhere in response)
    bool crcOk = false;
    for (auto b : response.data) {
        if (b == RSP_SUCCESS) { crcOk = true; break; }
    }

    Serial.printf("  [%s] CRC check: %s\n", side.name(), crcOk ? "OK" : "FAIL");
    return crcOk;
}

bool G1Device::_sendBmpData(G1Side& side, const uint8_t* data, size_t dataLen,
                            const uint8_t address[4]) {
    uint32_t totalPackets = (dataLen + BMP_PACKET_SIZE - 1) / BMP_PACKET_SIZE;
    Serial.printf("  [%s] Sending BMP: %d bytes, %d packets\n",
                  side.name(), (int)dataLen, (int)totalPackets);

    for (uint32_t i = 0; i < totalPackets; ++i) {
        uint32_t start = i * BMP_PACKET_SIZE;
        uint32_t end = (start + BMP_PACKET_SIZE < dataLen) ? start + BMP_PACKET_SIZE : dataLen;
        uint32_t chunkLen = end - start;

        // Build packet: [0x15, seq, address(4, first only), data]
        size_t headerSize = (i == 0) ? 6 : 2;  // cmd+seq(+address for first)
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
        if (!ok) {
            Serial.printf("  [%s] Write failed at packet %d/%d\n",
                          side.name(), (int)i + 1, (int)totalPackets);
            return false;
        }

        // Small delay between packets
        delay(8);

        if ((i + 1) % 10 == 0 || i == totalPackets - 1) {
            Serial.printf("  [%s] Packet %d/%d\n",
                          side.name(), (int)i + 1, (int)totalPackets);
        }
    }

    return true;
}

bool G1Device::sendBmpSync(const uint8_t* data, size_t dataLen,
                            const uint8_t address[4], int maxRetries) {
    for (int attempt = 0; attempt < maxRetries; attempt++) {
        if (attempt > 0) {
            Serial.printf("  Retry attempt %d/%d\n", attempt + 1, maxRetries);
            delay(500);
        }

        // Send to both sides sequentially
        bool leftOk = sendBmp(_left, data, dataLen, address);
        delay(200);
        bool rightOk = sendBmp(_right, data, dataLen, address);

        if (leftOk && rightOk) {
            return true;
        }

        // If one side failed, try to resync by sending heartbeat
        Serial.printf("  Sync failed L=%d R=%d, sending heartbeat...\n", leftOk, rightOk);
        heartbeat();
        delay(1000);
    }
    return false;
}

bool G1Device::sendText(const char* text) {
    std::string textStr(text);
    size_t len = textStr.size();
    constexpr size_t MAX_CHUNK = 191;
    uint32_t totalPackets = (len + MAX_CHUNK - 1) / MAX_CHUNK;

    Serial.printf("Sending text '%s' (%d bytes, %d packets)\n",
                  text, (int)len, (int)totalPackets);

    for (auto* side : {&_left, &_right}) {
        for (uint32_t i = 0; i < totalPackets; ++i) {
            size_t start = i * MAX_CHUNK;
            size_t chunkLen = (start + MAX_CHUNK < len) ? MAX_CHUNK : len - start;

            uint8_t pkt[256];
            pkt[0] = CMD_TEXT;
            pkt[1] = i & 0xFF;
            pkt[2] = totalPackets & 0xFF;
            pkt[3] = i & 0xFF;
            pkt[4] = SCREEN_TEXT;          // 0x71 = text + new content
            pkt[5] = 0x00;  // char_pos high
            pkt[6] = 0x00;  // char_pos low
            pkt[7] = 0x00;  // page
            pkt[8] = 0x01;  // max_page
            memcpy(pkt + 9, textStr.c_str() + start, chunkLen);

            side->write(pkt, 9 + chunkLen);
            delay(10);
        }
        delay(100);
    }

    return true;
}

}  // namespace eveng1
