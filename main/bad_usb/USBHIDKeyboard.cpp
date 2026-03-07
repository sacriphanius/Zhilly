#include "USBHIDKeyboard.h"
#include "esp_check.h"
#include "esp_timer.h"
#include "tinyusb.h"
#include "tinyusb_console.h"
#include "tusb_cdc_acm.h"
#ifndef SHIFT
#define SHIFT 0x80
#endif
#ifndef ALT_GR
#define ALT_GR 0x40
#endif
#ifndef ISO_REPLACEMENT
#define ISO_REPLACEMENT 0x32
#endif
#ifndef ISO_KEY
#define ISO_KEY 0x64
#endif
static const uint8_t hid_report_descriptor[] = {
    TUD_HID_REPORT_DESC_KEYBOARD(HID_REPORT_ID(HID_REPORT_ID_KEYBOARD))};
#define TUSB_DESC_TOTAL_LEN (TUD_CONFIG_DESC_LEN + TUD_CDC_DESC_LEN + TUD_HID_DESC_LEN)
static const uint8_t configuration_descriptor[] = {
    TUD_CONFIG_DESCRIPTOR(1, 3, 0, TUSB_DESC_TOTAL_LEN, TUSB_DESC_CONFIG_ATT_REMOTE_WAKEUP, 100),
    TUD_CDC_DESCRIPTOR(0, 4, 0x81, 8, 0x02, 0x82, 64),
    TUD_HID_DESCRIPTOR(2, 5, HID_ITF_PROTOCOL_KEYBOARD, sizeof(hid_report_descriptor), 0x83, 16,
                       10)};
static const tusb_desc_device_t device_descriptor = {.bLength = sizeof(tusb_desc_device_t),
                                                     .bDescriptorType = TUSB_DESC_DEVICE,
                                                     .bcdUSB = 0x0200,
                                                     .bDeviceClass = TUSB_CLASS_MISC,
                                                     .bDeviceSubClass = MISC_SUBCLASS_COMMON,
                                                     .bDeviceProtocol = MISC_PROTOCOL_IAD,
                                                     .bMaxPacketSize0 = CFG_TUD_ENDPOINT0_SIZE,
                                                     .idVendor = 0x303A,
                                                     .idProduct = 0x4005,
                                                     .bcdDevice = 0x0100,
                                                     .iManufacturer = 0x01,
                                                     .iProduct = 0x02,
                                                     .iSerialNumber = 0x03,
                                                     .bNumConfigurations = 0x01};
static const char* string_desc_arr[] = {
    (const char[]){0x09, 0x04},  
    "LilyGo",                    
    "T-Embed CC1101",            
    "123456",                    
    "T-Embed CDC",               
    "T-Embed Keyboard",          
};
USBHIDKeyboard::USBHIDKeyboard()
    : _asciimap(KeyboardLayout_en_US), shiftKeyReports(false), _cacheValid(false) {
    memset(&_keyReport, 0, sizeof(KeyReport));
    memset(_keySlotMap, 0, sizeof(_keySlotMap));
    memset(_shiftCache, 0, sizeof(_shiftCache));
    _buildShiftCache();
}
void USBHIDKeyboard::begin(const uint8_t* layout) {
    _asciimap = layout;
    _cacheValid = false;
    _buildShiftCache();
    const tinyusb_config_t tusb_cfg = {
        .port = (tinyusb_port_t)0,
        .phy = {.self_powered = false, .vbus_monitor_io = -1},
        .task = {.size = 4096, .priority = 5, .xCoreID = 0},
        .descriptor =
            {
                .device = &device_descriptor,
                .string = string_desc_arr,
                .string_count = sizeof(string_desc_arr) / sizeof(string_desc_arr[0]),
                .full_speed_config = configuration_descriptor,
            },
        .event_cb = NULL,
        .event_arg = NULL};
    ESP_LOGI("USBHID", "Installing TinyUSB driver with customized configuration...");
    esp_err_t err = tinyusb_driver_install(&tusb_cfg);
    if (err == ESP_OK) {
        ESP_LOGI("USBHID", "TinyUSB driver installed successfully.");
        esp_err_t console_err = tinyusb_console_init(0);
        if (console_err == ESP_OK) {
            ESP_LOGI("USBHID", "USB Console initialized on CDC interface 0.");
        }
    } else if (err == ESP_ERR_INVALID_STATE) {
        ESP_LOGW("USBHID", "TinyUSB driver already installed.");
    } else {
        ESP_LOGE("USBHID", "TinyUSB driver installation failed: %d", err);
    }
}
extern "C" uint8_t const* tud_hid_descriptor_report_cb(uint8_t instance) {
    (void)instance;
    return hid_report_descriptor;
}
void USBHIDKeyboard::end() {}
void USBHIDKeyboard::sendReport(KeyReport* keys) {
    if (!tud_hid_ready())
        return;
    uint64_t start_time = esp_timer_get_time();
    while (!tud_hid_ready()) {
        if (esp_timer_get_time() - start_time > 1000 * 100)
            return;  
        vTaskDelay(pdMS_TO_TICKS(1));
    }
    tud_hid_keyboard_report(HID_REPORT_ID_KEYBOARD, keys->modifiers, keys->keys);
}
size_t USBHIDKeyboard::pressRaw(uint8_t k) {
    uint8_t i;
    if (k >= 0xE0 && k < 0xE8) {
        _keyReport.modifiers |= (1 << (k - 0xE0));
    } else if (k && k < 0xA5) {
        int8_t emptySlot = -1;
        for (i = 0; i < 6; i++) {
            if (_keySlotMap[i] == k) {
                return 1;  
            }
            if (emptySlot == -1 && _keyReport.keys[i] == 0x00) {
                emptySlot = i;  
            }
        }
        if (emptySlot != -1) {
            _keyReport.keys[emptySlot] = k;
            _keySlotMap[emptySlot] = k;
        } else {
            return 0;  
        }
    } else if (_keyReport.modifiers == 0) {
        return 0;
    }
    sendReport(&_keyReport);
    return 1;
}
size_t USBHIDKeyboard::releaseRaw(uint8_t k) {
    uint8_t i;
    if (k >= 0xE0 && k < 0xE8) {
        _keyReport.modifiers &= ~(1 << (k - 0xE0));
    } else if (k && k < 0xA5) {
        for (i = 0; i < 6; i++) {
            if (_keySlotMap[i] == k) {
                _keyReport.keys[i] = 0x00;
                _keySlotMap[i] = 0x00;
                break;
            }
        }
    }
    sendReport(&_keyReport);
    return 1;
}
size_t USBHIDKeyboard::press(uint8_t k) {
    if (k >= 0x88) {  
        k = k - 0x88;
    } else if (k >= 0x80) {  
        _keyReport.modifiers |= (1 << (k - 0x80));
        k = 0;
    } else {  
        if (!_cacheValid) {
            _buildShiftCache();
        }
        k = _shiftCache[k];
        if (!k) {
            return 0;
        }
        if (k & SHIFT) {  
            if (shiftKeyReports) {
                pressRaw(HID_KEY_SHIFT_LEFT);
            } else {
                _keyReport.modifiers |= 0x02;  
            }
            k &= ~SHIFT;
        }
        if (k & ALT_GR) {
            _keyReport.modifiers |= 0x40;  
            k &= ~ALT_GR;
        }
        if (k == ISO_REPLACEMENT) {
            k = ISO_KEY;
        }
    }
    return pressRaw(k);
}
size_t USBHIDKeyboard::release(uint8_t k) {
    if (k >= 0x88) {  
        k = k - 0x88;
    } else if (k >= 0x80) {  
        _keyReport.modifiers &= ~(1 << (k - 0x80));
        k = 0;
    } else {  
        if (!_cacheValid) {
            _buildShiftCache();
        }
        k = _shiftCache[k];
        if (!k) {
            return 0;
        }
        if (k & SHIFT) {  
            if (shiftKeyReports) {
                releaseRaw(k & 0x7F);    
                k = HID_KEY_SHIFT_LEFT;  
            } else {
                _keyReport.modifiers &= ~(0x02);  
                k &= ~SHIFT;
            }
        }
        if (k & ALT_GR) {
            _keyReport.modifiers &= ~(0x40);  
            k &= ~ALT_GR;
        }
        if (k == ISO_REPLACEMENT) {
            k = ISO_KEY;
        }
    }
    return releaseRaw(k);
}
void USBHIDKeyboard::releaseAll(void) {
    memset(&_keyReport, 0, sizeof(KeyReport));
    memset(_keySlotMap, 0, sizeof(_keySlotMap));
    sendReport(&_keyReport);
}
size_t USBHIDKeyboard::write(uint8_t c) {
    uint8_t p = press(c);
    vTaskDelay(pdMS_TO_TICKS(this->_delay_ms ? this->_delay_ms : 1));
    release(c);
    vTaskDelay(pdMS_TO_TICKS(this->_delay_ms ? this->_delay_ms : 1));
    return p;
}
extern "C" uint16_t tud_hid_get_report_cb(uint8_t instance, uint8_t report_id,
                                          hid_report_type_t report_type, uint8_t* buffer,
                                          uint16_t reqlen) {
    (void)instance;
    (void)report_id;
    (void)report_type;
    (void)buffer;
    (void)reqlen;
    return 0;
}
extern "C" void tud_hid_set_report_cb(uint8_t instance, uint8_t report_id,
                                      hid_report_type_t report_type, uint8_t const* buffer,
                                      uint16_t bufsize) {
    (void)instance;
    (void)report_id;
    (void)report_type;
    (void)buffer;
    (void)bufsize;
}
size_t USBHIDKeyboard::write(const uint8_t* buffer, size_t size) {
    size_t n = 0;
    while (size--) {
        if (*buffer != '\r') {
            if (write(*buffer)) {
                n++;
            } else {
                break;
            }
        }
        buffer++;
    }
    return n;
}
void USBHIDKeyboard::_buildShiftCache() {
    for (uint8_t i = 0; i < 128; i++) {
        uint8_t mapValue = _asciimap[i];
        _shiftCache[i] = mapValue;
    }
    _cacheValid = true;
}
