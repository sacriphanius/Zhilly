#pragma once
#include "Bad_Usb_Lib.h"
#include "keys.h"
#include "tinyusb.h"
#include "class/hid/hid_device.h"
#define HID_REPORT_ID_KEYBOARD 1
class USBHIDKeyboard : public HIDInterface {
private:
    KeyReport _keyReport;
    const uint8_t *_asciimap;
    bool shiftKeyReports;
    uint32_t _delay_ms = 10;
    uint8_t _keySlotMap[6];
    uint8_t _shiftCache[128];
    bool _cacheValid;
    void _buildShiftCache();
    size_t pressRaw(uint8_t k);
    size_t releaseRaw(uint8_t k);
    void sendReport(KeyReport *keys);
public:
    USBHIDKeyboard();
    void begin(const uint8_t *layout = KeyboardLayout_en_US) override;
    void end(void) override;
    size_t write(uint8_t k) override;
    size_t write(const uint8_t *buffer, size_t size) override;
    size_t press(uint8_t k) override;
    size_t release(uint8_t k) override;
    void releaseAll(void) override;
    void setLayout(const uint8_t *layout) override {
        _asciimap = layout;
        _cacheValid = false;
        _buildShiftCache();
    }
    void setDelay(uint32_t ms) override {
        _delay_ms = ms;
    }
    bool isConnected() override {
        return tud_hid_ready();
    }
};
