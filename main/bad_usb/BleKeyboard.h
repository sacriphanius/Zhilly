#pragma once

#include "sdkconfig.h"
#if defined(CONFIG_BT_ENABLED)

#include <string>
#include <vector>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include "NimBLECharacteristic.h"
#include "NimBLEHIDDevice.h"
#include "NimBLEAdvertising.h"
#include "NimBLEServer.h"

#include "keys.h"
#include "bad_usb_service.h" // For KeyboardLayout or HIDInterface if necessary, we will pull in dependencies.

// We will redefine layouts and keys as needed without relying on Arduino's Print.h
// Media keys and HID report structs:

class BleKeyboard : public HIDInterface {
private:
    NimBLEHIDDevice *hid;
    NimBLECharacteristic *inputKeyboard;
    NimBLECharacteristic *outputKeyboard;
    NimBLECharacteristic *inputMediaKeys;
    NimBLEAdvertising *advertising;
    NimBLEServer *pServer;
    
    KeyReport _keyReport;
    MediaKeyReport _mediaKeyReport;
    
    std::string deviceName;
    std::string deviceManufacturer;
    uint8_t batteryLevel;
    bool connected = false;
    uint32_t _delay_ms = 7;
    void delay_ms(uint64_t ms);

    uint16_t vid = 0x05ac;
    uint16_t pid = 0x820a;
    uint16_t version = 0x0210;
    uint16_t appearance = 0x03C1;

    const uint8_t *_asciimap;

    class ServerCallbacks : public NimBLEServerCallbacks {
    private:
        BleKeyboard *parent;
    public:
        ServerCallbacks(BleKeyboard *kb) : parent(kb) {}
        void onConnect(NimBLEServer *pServer, NimBLEConnInfo &connInfo) override;
        void onDisconnect(NimBLEServer *pServer, NimBLEConnInfo &connInfo, int reason) override;
        void onAuthenticationComplete(NimBLEConnInfo &connInfo) override;
    };
    
    class CharacteristicCallbacks : public NimBLECharacteristicCallbacks {
    private:
        BleKeyboard *parent;
    public:
        CharacteristicCallbacks(BleKeyboard *kb) : parent(kb) {}
        void onWrite(NimBLECharacteristic *pCharacteristic, NimBLEConnInfo &connInfo) override;
        void onSubscribe(NimBLECharacteristic *pCharacteristic, NimBLEConnInfo &connInfo, uint16_t subValue) override;
    };
    
    uint8_t m_subCount{0};
    uint8_t getSubscribedCount() { return m_subCount; }
    friend class ServerCallbacks;
    friend class CharacteristicCallbacks;

public:
    BleKeyboard(std::string deviceName = "ESP32 Keyboard", std::string deviceManufacturer = "Espressif", uint8_t batteryLevel = 100);
    virtual ~BleKeyboard();

    void begin(const uint8_t *layout = nullptr) override;
    void end() override;
    void sendReport(KeyReport *keys);
    void sendReport(MediaKeyReport *keys);
    
    // HIDInterfaceble impl
    size_t press(uint8_t k) override;
    size_t release(uint8_t k) override;
    size_t write(uint8_t c) override;
    size_t write(const uint8_t *buffer, size_t size) override;
    void releaseAll(void) override;
    
    size_t press(const MediaKeyReport k);
    size_t release(const MediaKeyReport k);
    size_t write(const MediaKeyReport c);
    
    bool isConnected(void) override;
    void setBatteryLevel(uint8_t level);
    void setName(std::string deviceName);
    void setDelay(uint32_t ms) override;
    
    void set_vendor_id(uint16_t vid);
    void set_product_id(uint16_t pid);
    void set_version(uint16_t version);
};

#endif // CONFIG_BT_ENABLED
