#pragma once

#include "../devices/device.hpp"
#include <string>
#include <vector>
#include <functional>
#include <memory>
#include <mutex>
#include <condition_variable>
#include <dbus/dbus.h>

namespace eveng1 {

class BleBlueZ : public BleTransport {
public:
    BleBlueZ();
    ~BleBlueZ() override;

    bool connect(const char* address) override;
    void disconnect() override;
    bool isConnected() const override;

    bool write(const uint8_t* data, uint16_t len) override;
    void setNotifyCallback(NotifyCallback cb) override;
    uint8_t* waitForNotify(uint16_t& outLen, uint32_t timeoutMs) override;

private:
    bool initDBus();
    void cleanupDBus();
    bool findDevice(const char* address);
    bool connectDevice();
    bool discoverServices();
    bool findNUSCharacteristics();
    bool subscribeNotifications();

    static DBusHandlerResult messageFilter(DBusConnection* conn, DBusMessage* msg, void* data);

    bool parseUUID(const char* str, uint8_t out[16]);
    std::string uuidToString(const uint8_t uuid[16]);
    bool uuidMatches(const uint8_t a[16], const uint8_t b[16]);

    DBusConnection* m_conn = nullptr;
    std::string m_devicePath;
    std::string m_txPath;  // Write characteristic
    std::string m_rxPath;  // Notify characteristic

    NotifyCallback m_notifyCb;
    std::mutex m_notifyMutex;
    std::condition_variable m_notifyCv;
    std::vector<uint8_t> m_notifyData;
    bool m_notifyReady = false;

    bool m_connected = false;
};

}  // namespace eveng1
