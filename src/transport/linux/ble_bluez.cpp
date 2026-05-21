#include "eveng1/transport/ble_bluez.hpp"
#include "eveng1/protocol/g1_commands.hpp"
#include <cstring>
#include <iostream>
#include <thread>
#include <chrono>

#define BLUEZ_SERVICE "org.bluez"
#define ADAPTER_INTERFACE "org.bluez.Adapter1"
#define DEVICE_INTERFACE "org.bluez.Device1"
#define GATT_SERVICE_INTERFACE "org.bluez.GattService1"
#define GATT_CHAR_INTERFACE "org.bluez.GattCharacteristic1"
#define PROPERTIES_INTERFACE "org.freedesktop.DBus.Properties"
#define OBJECT_MANAGER_INTERFACE "org.freedesktop.DBus.ObjectManager"
#define DBUS_PATH "/"

namespace eveng1 {

BleBlueZ::BleBlueZ() = default;

BleBlueZ::~BleBlueZ() {
    disconnect();
    cleanupDBus();
}

bool BleBlueZ::initDBus() {
    if (m_conn) return true;

    DBusError err;
    dbus_error_init(&err);

    m_conn = dbus_bus_get(DBUS_BUS_SYSTEM, &err);
    if (!m_conn) {
        std::cerr << "DBus error: " << err.message << std::endl;
        dbus_error_free(&err);
        return false;
    }

    return true;
}

void BleBlueZ::cleanupDBus() {
    if (m_conn) {
        dbus_connection_unref(m_conn);
        m_conn = nullptr;
    }
}

bool BleBlueZ::connect(const char* address) {
    if (!initDBus()) return false;

    if (!findDevice(address)) {
        std::cerr << "Device not found: " << address << std::endl;
        return false;
    }

    if (!connectDevice()) {
        std::cerr << "Failed to connect to device" << std::endl;
        return false;
    }

    // BlueZ 5.x auto-discovers services on connect, skip DiscoverServices
    // if (!discoverServices()) {
    //     std::cerr << "Failed to discover services" << std::endl;
    //     return false;
    // }

    if (!findNUSCharacteristics()) {
        std::cerr << "NUS characteristics not found" << std::endl;
        return false;
    }

    if (!subscribeNotifications()) {
        std::cerr << "Failed to subscribe to notifications" << std::endl;
        return false;
    }

    m_connected = true;
    return true;
}

void BleBlueZ::disconnect() {
    if (!m_connected) return;

    if (!m_devicePath.empty() && m_conn) {
        DBusMessage* msg = dbus_message_new_method_call(
            BLUEZ_SERVICE, m_devicePath.c_str(),
            DEVICE_INTERFACE, "Disconnect");
        if (msg) {
            dbus_connection_send(m_conn, msg, nullptr);
            dbus_message_unref(msg);
        }
    }

    m_connected = false;
    m_devicePath.clear();
    m_txPath.clear();
    m_rxPath.clear();
}

bool BleBlueZ::isConnected() const {
    return m_connected;
}

bool BleBlueZ::write(const uint8_t* data, uint16_t len) {
    if (!m_connected || m_txPath.empty() || !m_conn) return false;

    DBusMessage* msg = dbus_message_new_method_call(
        BLUEZ_SERVICE, m_txPath.c_str(),
        GATT_CHAR_INTERFACE, "WriteValue");
    if (!msg) return false;

    DBusMessageIter iter, arrIter;
    dbus_message_iter_init_append(msg, &iter);

    dbus_message_iter_open_container(&iter, DBUS_TYPE_ARRAY, DBUS_TYPE_BYTE_AS_STRING, &arrIter);
    for (uint16_t i = 0; i < len; i++) {
        dbus_message_iter_append_basic(&arrIter, DBUS_TYPE_BYTE, &data[i]);
    }
    dbus_message_iter_close_container(&iter, &arrIter);

    // Options dict: {"type": "request"}
    DBusMessageIter dictIter;
    dbus_message_iter_open_container(&iter, DBUS_TYPE_ARRAY, "{sv}", &dictIter);

    DBusMessageIter dictEntryIter;
    dbus_message_iter_open_container(&dictIter, DBUS_TYPE_DICT_ENTRY, nullptr, &dictEntryIter);

    const char* key = "type";
    dbus_message_iter_append_basic(&dictEntryIter, DBUS_TYPE_STRING, &key);

    DBusMessageIter variantIter;
    dbus_message_iter_open_container(&dictEntryIter, DBUS_TYPE_VARIANT, "s", &variantIter);
    const char* typeVal = "command";  // WriteWithoutResponse
    dbus_message_iter_append_basic(&variantIter, DBUS_TYPE_STRING, &typeVal);
    dbus_message_iter_close_container(&dictEntryIter, &variantIter);

    dbus_message_iter_close_container(&dictIter, &dictEntryIter);
    dbus_message_iter_close_container(&iter, &dictIter);

    DBusError err;
    dbus_error_init(&err);
    DBusMessage* reply = dbus_connection_send_with_reply_and_block(m_conn, msg, 5000, &err);
    dbus_message_unref(msg);

    if (!reply) {
        std::cerr << "Write error: " << err.message << std::endl;
        dbus_error_free(&err);
        return false;
    }
    dbus_message_unref(reply);
    return true;
}

void BleBlueZ::setNotifyCallback(NotifyCallback cb) {
    m_notifyCb = cb;
}

uint8_t* BleBlueZ::waitForNotify(uint16_t& outLen, uint32_t timeoutMs) {
    std::unique_lock<std::mutex> lock(m_notifyMutex);
    if (!m_notifyCv.wait_for(lock, std::chrono::milliseconds(timeoutMs), [this]{ return m_notifyReady; })) {
        outLen = 0;
        return nullptr;
    }

    outLen = static_cast<uint16_t>(m_notifyData.size());
    auto* result = new uint8_t[outLen];
    std::memcpy(result, m_notifyData.data(), outLen);
    m_notifyData.clear();
    m_notifyReady = false;
    return result;
}

bool BleBlueZ::findDevice(const char* address) {
    if (!m_conn) return false;

    // Get managed objects to find all Bluetooth devices
    DBusMessage* msg = dbus_message_new_method_call(
        BLUEZ_SERVICE, DBUS_PATH,
        OBJECT_MANAGER_INTERFACE, "GetManagedObjects");
    if (!msg) return false;

    DBusMessage* reply = dbus_connection_send_with_reply_and_block(m_conn, msg, 5000, nullptr);
    dbus_message_unref(msg);
    if (!reply) return false;

    bool found = false;
    DBusMessageIter iter, dictIter;
    if (dbus_message_iter_init(reply, &iter)) {
        // Array of {object_path, interfaces_and_properties}
        if (dbus_message_iter_get_arg_type(&iter) == DBUS_TYPE_ARRAY) {
            DBusMessageIter arrIter;
            dbus_message_iter_recurse(&iter, &arrIter);
            while (dbus_message_iter_get_arg_type(&arrIter) == DBUS_TYPE_DICT_ENTRY) {
                DBusMessageIter entryIter;
                dbus_message_iter_recurse(&arrIter, &entryIter);

                const char* path = nullptr;
                dbus_message_iter_get_basic(&entryIter, &path);
                dbus_message_iter_next(&entryIter);

                // Check if this is a Device1 interface
                DBusMessageIter ifaceDictIter;
                dbus_message_iter_recurse(&entryIter, &ifaceDictIter);
                while (dbus_message_iter_get_arg_type(&ifaceDictIter) == DBUS_TYPE_DICT_ENTRY) {
                    DBusMessageIter ifaceEntryIter;
                    dbus_message_iter_recurse(&ifaceDictIter, &ifaceEntryIter);

                    const char* ifaceName = nullptr;
                    dbus_message_iter_get_basic(&ifaceEntryIter, &ifaceName);
                    dbus_message_iter_next(&ifaceEntryIter);

                    if (strcmp(ifaceName, DEVICE_INTERFACE) == 0) {
                        // Get Address property
                        DBusMessageIter propDictIter;
                        dbus_message_iter_recurse(&ifaceEntryIter, &propDictIter);
                        while (dbus_message_iter_get_arg_type(&propDictIter) == DBUS_TYPE_DICT_ENTRY) {
                            DBusMessageIter propEntryIter;
                            dbus_message_iter_recurse(&propDictIter, &propEntryIter);

                            const char* propName = nullptr;
                            dbus_message_iter_get_basic(&propEntryIter, &propName);
                            dbus_message_iter_next(&propEntryIter);

                            if (strcmp(propName, "Address") == 0) {
                                DBusMessageIter variantIter;
                                dbus_message_iter_recurse(&propEntryIter, &variantIter);
                                const char* devAddr = nullptr;
                                dbus_message_iter_get_basic(&variantIter, &devAddr);
                                if (devAddr && strcasecmp(devAddr, address) == 0) {
                                    m_devicePath = path;
                                    found = true;
                                }
                            }
                            dbus_message_iter_next(&propDictIter);
                        }
                    }
                    dbus_message_iter_next(&ifaceDictIter);
                }

                if (found) break;
                dbus_message_iter_next(&arrIter);
            }
        }
    }

    dbus_message_unref(reply);
    return found;
}

bool BleBlueZ::connectDevice() {
    if (m_devicePath.empty() || !m_conn) return false;

    DBusMessage* msg = dbus_message_new_method_call(
        BLUEZ_SERVICE, m_devicePath.c_str(),
        DEVICE_INTERFACE, "Connect");
    if (!msg) return false;

    DBusError err;
    dbus_error_init(&err);
    DBusMessage* reply = dbus_connection_send_with_reply_and_block(m_conn, msg, 30000, &err);
    dbus_message_unref(msg);

    if (!reply) {
        std::cerr << "Connect error: " << err.message << std::endl;
        dbus_error_free(&err);
        return false;
    }

    dbus_message_unref(reply);

    // Wait for BlueZ to resolve GATT services
    std::this_thread::sleep_for(std::chrono::seconds(2));

    return true;
}

bool BleBlueZ::discoverServices() {
    if (m_devicePath.empty() || !m_conn) return false;

    DBusMessage* msg = dbus_message_new_method_call(
        BLUEZ_SERVICE, m_devicePath.c_str(),
        DEVICE_INTERFACE, "DiscoverServices");
    if (!msg) return false;

    DBusError err;
    dbus_error_init(&err);
    DBusMessage* reply = dbus_connection_send_with_reply_and_block(m_conn, msg, 10000, &err);
    dbus_message_unref(msg);

    if (!reply) {
        std::cerr << "DiscoverServices error: " << err.message << std::endl;
        dbus_error_free(&err);
        return false;
    }

    dbus_message_unref(reply);
    return true;
}

static bool uuidEqual(const uint8_t* a, const uint8_t* b) {
    return std::memcmp(a, b, 16) == 0;
}

static void parseUUID128(const char* str, uint8_t out[16]) {
    std::memset(out, 0, 16);
    int len = std::strlen(str);
    if (len == 36) {
        // Full 128-bit UUID: xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx
        for (int i = 0; i < 16; i++) {
            int p1 = (i < 4) ? i * 2 : (i < 6) ? i * 2 + 1 : (i < 8) ? i * 2 + 2 : (i < 10) ? i * 2 + 3 : i * 2 + 4;
            int p2 = p1 + 1;
            // Skip dashes
            if (str[p1] == '-') p1++;
            if (str[p2] == '-') p2++;
            unsigned int byte;
            sscanf(str + p1, "%2x", &byte);
            out[15 - i] = static_cast<uint8_t>(byte);  // Little-endian
        }
    } else if (len == 4) {
        // 16-bit UUID
        unsigned int val;
        sscanf(str, "%4x", &val);
        out[0] = val & 0xFF;
        out[1] = (val >> 8) & 0xFF;
    }
}

bool BleBlueZ::findNUSCharacteristics() {
    if (!m_conn || m_devicePath.empty()) return false;

    uint8_t nusServiceUUID[16], txUUID[16], rxUUID[16];
    parseUUID128("6E400001-B5A3-F393-E0A9-E50E24DCCA9E", nusServiceUUID);
    parseUUID128("6E400002-B5A3-F393-E0A9-E50E24DCCA9E", txUUID);
    parseUUID128("6E400003-B5A3-F393-E0A9-E50E24DCCA9E", rxUUID);

    // Get managed objects again to find GATT characteristics
    DBusMessage* msg = dbus_message_new_method_call(
        BLUEZ_SERVICE, DBUS_PATH,
        OBJECT_MANAGER_INTERFACE, "GetManagedObjects");
    if (!msg) return false;

    DBusMessage* reply = dbus_connection_send_with_reply_and_block(m_conn, msg, 5000, nullptr);
    dbus_message_unref(msg);
    if (!reply) return false;

    bool found = false;
    std::string nusServicePath;

    std::cerr << "Looking for NUS characteristics..." << std::endl;

    DBusMessageIter iter;
    if (dbus_message_iter_init(reply, &iter) &&
        dbus_message_iter_get_arg_type(&iter) == DBUS_TYPE_ARRAY) {
        DBusMessageIter arrIter;
        dbus_message_iter_recurse(&iter, &arrIter);
        while (dbus_message_iter_get_arg_type(&arrIter) == DBUS_TYPE_DICT_ENTRY) {
            DBusMessageIter entryIter;
            dbus_message_iter_recurse(&arrIter, &entryIter);

            const char* path = nullptr;
            dbus_message_iter_get_basic(&entryIter, &path);
            dbus_message_iter_next(&entryIter);

            DBusMessageIter ifaceDictIter;
            dbus_message_iter_recurse(&entryIter, &ifaceDictIter);
            while (dbus_message_iter_get_arg_type(&ifaceDictIter) == DBUS_TYPE_DICT_ENTRY) {
                DBusMessageIter ifaceEntryIter;
                dbus_message_iter_recurse(&ifaceDictIter, &ifaceEntryIter);

                const char* ifaceName = nullptr;
                dbus_message_iter_get_basic(&ifaceEntryIter, &ifaceName);
                dbus_message_iter_next(&ifaceEntryIter);

                if (strcmp(ifaceName, GATT_SERVICE_INTERFACE) == 0) {
                    // Check UUID
                    DBusMessageIter propDictIter;
                    dbus_message_iter_recurse(&ifaceEntryIter, &propDictIter);
                    while (dbus_message_iter_get_arg_type(&propDictIter) == DBUS_TYPE_DICT_ENTRY) {
                        DBusMessageIter propEntryIter;
                        dbus_message_iter_recurse(&propDictIter, &propEntryIter);

                        const char* propName = nullptr;
                        dbus_message_iter_get_basic(&propEntryIter, &propName);
                        dbus_message_iter_next(&propEntryIter);

                        if (strcmp(propName, "UUID") == 0) {
                            DBusMessageIter variantIter;
                            dbus_message_iter_recurse(&propEntryIter, &variantIter);
                            const char* uuidStr = nullptr;
                            dbus_message_iter_get_basic(&variantIter, &uuidStr);
                            if (uuidStr) {
                                uint8_t uuid[16];
                                parseUUID128(uuidStr, uuid);
                                if (uuidEqual(uuid, nusServiceUUID)) {
                                    nusServicePath = path;
                                }
                            }
                        }
                        dbus_message_iter_next(&propDictIter);
                    }
                }

                if (strcmp(ifaceName, GATT_CHAR_INTERFACE) == 0) {
                    // Check if this characteristic belongs to our NUS service
                    if (!nusServicePath.empty() && std::string(path).find(nusServicePath) == 0) {
                        DBusMessageIter propDictIter;
                        dbus_message_iter_recurse(&ifaceEntryIter, &propDictIter);
                        while (dbus_message_iter_get_arg_type(&propDictIter) == DBUS_TYPE_DICT_ENTRY) {
                            DBusMessageIter propEntryIter;
                            dbus_message_iter_recurse(&propDictIter, &propEntryIter);

                            const char* propName = nullptr;
                            dbus_message_iter_get_basic(&propEntryIter, &propName);
                            dbus_message_iter_next(&propEntryIter);

                            if (strcmp(propName, "UUID") == 0) {
                                DBusMessageIter variantIter;
                                dbus_message_iter_recurse(&propEntryIter, &variantIter);
                                const char* uuidStr = nullptr;
                                dbus_message_iter_get_basic(&variantIter, &uuidStr);
                                if (uuidStr) {
                                    uint8_t uuid[16];
                                    parseUUID128(uuidStr, uuid);
                                    if (uuidEqual(uuid, txUUID)) {
                                        m_txPath = path;
                                        std::cerr << "  TX: " << path << std::endl;
                                    } else if (uuidEqual(uuid, rxUUID)) {
                                        m_rxPath = path;
                                        std::cerr << "  RX: " << path << std::endl;
                                    }
                                }
                            }
                            dbus_message_iter_next(&propDictIter);
                        }
                    }
                }

                dbus_message_iter_next(&ifaceDictIter);
            }

            dbus_message_iter_next(&arrIter);
        }
    }

    dbus_message_unref(reply);
    found = !m_txPath.empty() && !m_rxPath.empty();
    return found;
}

bool BleBlueZ::subscribeNotifications() {
    if (m_rxPath.empty() || !m_conn) return false;

    // Add filter for PropertiesChanged signals
    DBusError err;
    dbus_error_init(&err);
    dbus_bus_add_match(m_conn,
        "type='signal',interface='org.freedesktop.DBus.Properties',member='PropertiesChanged'",
        &err);
    if (dbus_error_is_set(&err)) {
        std::cerr << "DBus match error: " << err.message << std::endl;
        dbus_error_free(&err);
        return false;
    }

    // Register message filter
    dbus_connection_add_filter(m_conn, BleBlueZ::messageFilter, this, nullptr);

    // Write CCCD descriptor manually (0x2902) to enable notifications
    // BlueZ sometimes doesn't auto-enable notifications
    DBusMessage* descMsg = dbus_message_new_method_call(
        BLUEZ_SERVICE, (m_rxPath + "/desc0001").c_str(),
        "org.bluez.GattDescriptor1", "WriteValue");
    if (descMsg) {
        DBusMessageIter iter, arrIter;
        dbus_message_iter_init_append(descMsg, &iter);
        dbus_message_iter_open_container(&iter, DBUS_TYPE_ARRAY, DBUS_TYPE_BYTE_AS_STRING, &arrIter);
        uint8_t cccd[2] = {0x01, 0x00};  // Enable notifications
        for (int i = 0; i < 2; i++) {
            dbus_message_iter_append_basic(&arrIter, DBUS_TYPE_BYTE, &cccd[i]);
        }
        dbus_message_iter_close_container(&iter, &arrIter);

        // Options dict
        DBusMessageIter dictIter;
        dbus_message_iter_open_container(&iter, DBUS_TYPE_ARRAY, "{sv}", &dictIter);
        dbus_message_iter_close_container(&iter, &dictIter);

        DBusMessage* descReply = dbus_connection_send_with_reply_and_block(m_conn, descMsg, 2000, nullptr);
        dbus_message_unref(descMsg);
        if (descReply) dbus_message_unref(descReply);
    }

    // Start notifications via BlueZ
    DBusMessage* msg = dbus_message_new_method_call(
        BLUEZ_SERVICE, m_rxPath.c_str(),
        GATT_CHAR_INTERFACE, "StartNotify");
    if (!msg) return false;

    DBusMessage* reply = dbus_connection_send_with_reply_and_block(m_conn, msg, 5000, nullptr);
    dbus_message_unref(msg);

    if (!reply) return false;
    dbus_message_unref(reply);
    return true;
}

DBusHandlerResult BleBlueZ::messageFilter(DBusConnection* conn, DBusMessage* msg, void* data) {
    auto* self = static_cast<BleBlueZ*>(data);

    if (dbus_message_is_signal(msg, PROPERTIES_INTERFACE, "PropertiesChanged")) {
        DBusMessageIter iter;
        if (!dbus_message_iter_init(msg, &iter))
            return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;

        const char* ifaceName = nullptr;
        dbus_message_iter_get_basic(&iter, &ifaceName);
        if (!ifaceName || strcmp(ifaceName, GATT_CHAR_INTERFACE) != 0)
            return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;

        dbus_message_iter_next(&iter);  // skip changed properties
        dbus_message_iter_next(&iter);  // skip invalidated

        // Check if this is for our RX characteristic
        const char* sender = dbus_message_get_sender(msg);
        const char* path = dbus_message_get_path(msg);
        if (!path || self->m_rxPath != path)
            return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;

        // Get the value from changed properties dict
        // Actually, for GattCharacteristic1, the Value property is sent separately
        // We need to read the Value property
        DBusMessage* readMsg = dbus_message_new_method_call(
            BLUEZ_SERVICE, path,
            PROPERTIES_INTERFACE, "Get");
        if (readMsg) {
            const char* iface = GATT_CHAR_INTERFACE;
            const char* prop = "Value";
            dbus_message_append_args(readMsg,
                DBUS_TYPE_STRING, &iface,
                DBUS_TYPE_STRING, &prop,
                DBUS_TYPE_INVALID);

            DBusMessage* reply = dbus_connection_send_with_reply_and_block(conn, readMsg, 1000, nullptr);
            dbus_message_unref(readMsg);

            if (reply) {
                DBusMessageIter replyIter;
                if (dbus_message_iter_init(reply, &replyIter)) {
                    DBusMessageIter variantIter;
                    dbus_message_iter_recurse(&replyIter, &variantIter);
                    if (dbus_message_iter_get_arg_type(&variantIter) == DBUS_TYPE_ARRAY) {
                        DBusMessageIter arrIter;
                        dbus_message_iter_recurse(&variantIter, &arrIter);
                        std::vector<uint8_t> value;
                        while (dbus_message_iter_get_arg_type(&arrIter) == DBUS_TYPE_BYTE) {
                            uint8_t byte;
                            dbus_message_iter_get_basic(&arrIter, &byte);
                            value.push_back(byte);
                            dbus_message_iter_next(&arrIter);
                        }

                        if (!value.empty() && self->m_notifyCb) {
                            // Parse the BLE packet: first byte is command
                            uint8_t cmd = value[0];
                            self->m_notifyCb(cmd, value.data() + 1, static_cast<uint16_t>(value.size() - 1));
                        }

                        std::lock_guard<std::mutex> lock(self->m_notifyMutex);
                        self->m_notifyData = value;
                        self->m_notifyReady = true;
                        self->m_notifyCv.notify_one();
                    }
                }
                dbus_message_unref(reply);
            }
        }

        return DBUS_HANDLER_RESULT_HANDLED;
    }

    return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
}

}  // namespace eveng1
