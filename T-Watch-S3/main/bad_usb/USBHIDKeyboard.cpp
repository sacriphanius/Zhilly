
#include "USBHIDKeyboard.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "tinyusb.h"
#include "class/hid/hid_device.h"
#include "hal/usb_serial_jtag_ll.h"

static const char* TAG = "USBHID";

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
    TUD_HID_REPORT_DESC_KEYBOARD(HID_REPORT_ID(HID_REPORT_ID_KEYBOARD))
};

static const tusb_desc_device_t device_descriptor = {
    .bLength            = sizeof(tusb_desc_device_t),
    .bDescriptorType    = TUSB_DESC_DEVICE,
    .bcdUSB             = 0x0200,
    .bDeviceClass       = 0x00,

    .bDeviceSubClass    = 0x00,
    .bDeviceProtocol    = 0x00,
    .bMaxPacketSize0    = CFG_TUD_ENDPOINT0_SIZE,
    .idVendor           = 0x303A,
    .idProduct          = 0x4004,
    .bcdDevice          = 0x0100,
    .iManufacturer      = 0x01,
    .iProduct           = 0x02,
    .iSerialNumber      = 0x03,
    .bNumConfigurations = 0x01
};

#define TUSB_DESC_TOTAL_LEN (TUD_CONFIG_DESC_LEN + TUD_HID_DESC_LEN)
static const uint8_t configuration_descriptor[] = {
    TUD_CONFIG_DESCRIPTOR(1, 1, 0, TUSB_DESC_TOTAL_LEN, TUSB_DESC_CONFIG_ATT_REMOTE_WAKEUP, 100),
    TUD_HID_DESCRIPTOR(0, 4, HID_ITF_PROTOCOL_KEYBOARD, sizeof(hid_report_descriptor), 0x81, 16, 1)
};

static const char* string_desc_arr[] = {
    "LilyGo",
    "T-Embed CC1101 HID",
    "BAD-USB-001",
    "T-Embed HID",
};

static SemaphoreHandle_t s_report_sem   = NULL;
static SemaphoreHandle_t s_report_mutex = NULL;
static bool              s_installed    = false;

extern "C" uint8_t const* tud_hid_descriptor_report_cb(uint8_t instance) {
    (void)instance;
    return hid_report_descriptor;
}

extern "C" uint16_t tud_hid_get_report_cb(uint8_t instance, uint8_t report_id,
    hid_report_type_t report_type, uint8_t* buffer, uint16_t reqlen) {
    (void)instance; (void)report_id; (void)report_type; (void)buffer; (void)reqlen;
    return 0;
}

extern "C" void tud_hid_set_report_cb(uint8_t instance, uint8_t report_id,
    hid_report_type_t report_type, uint8_t const* buffer, uint16_t bufsize) {
    (void)instance; (void)report_id; (void)report_type; (void)buffer; (void)bufsize;
}

extern "C" void tud_hid_report_complete_cb(uint8_t instance, uint8_t const* report, uint16_t len) {
    (void)instance; (void)report; (void)len;
    if (s_report_sem) {
        xSemaphoreGiveFromISR(s_report_sem, NULL);
    }
}

#include "KeyboardLayout.inc"

USBHIDKeyboard::USBHIDKeyboard()
    : _asciimap(KeyboardLayout_en_US), shiftKeyReports(false), _cacheValid(false) {
    memset(&_keyReport, 0, sizeof(KeyReport));
    memset(_keySlotMap, 0, sizeof(_keySlotMap));
    memset(_shiftCache, 0, sizeof(_shiftCache));
    _buildShiftCache();
}

void USBHIDKeyboard::begin(const uint8_t* layout) {
    _asciimap = layout ? layout : KeyboardLayout_en_US;
    _cacheValid = false;
    _buildShiftCache();

    if (s_installed) {
        ESP_LOGW(TAG, "TinyUSB already installed, skipping.");
        return;
    }

    if (!s_report_sem)   s_report_sem   = xSemaphoreCreateBinary();
    if (!s_report_mutex) s_report_mutex = xSemaphoreCreateMutex();

    const tinyusb_config_t tusb_cfg = {
        .port = TINYUSB_PORT_FULL_SPEED_0,
        .phy = {
            .skip_setup      = false,
            .self_powered    = false,
            .vbus_monitor_io = -1
        },
        .task = {
            .size    = 4096,
            .priority = 5,
            .xCoreID = 0
        },
        .descriptor = {
            .device           = &device_descriptor,
            .qualifier        = NULL,
            .string           = string_desc_arr,
            .string_count     = (int)(sizeof(string_desc_arr) / sizeof(string_desc_arr[0])),
            .full_speed_config = configuration_descriptor,
            .high_speed_config = NULL
        },
        .event_cb  = NULL,
        .event_arg = NULL
    };

    esp_err_t err = tinyusb_driver_install(&tusb_cfg);
    if (err == ESP_OK) {
        s_installed = true;
        ESP_LOGI(TAG, "TinyUSB (HID-only) installed successfully.");
    } else if (err == ESP_ERR_INVALID_STATE) {
        s_installed = true;
        ESP_LOGW(TAG, "TinyUSB already installed.");
    } else {
        ESP_LOGE(TAG, "TinyUSB install failed: 0x%x", err);
    }
}

void USBHIDKeyboard::end() {
    if (!s_installed) return;

    ESP_LOGI(TAG, "Uninstalling TinyUSB driver...");

    esp_err_t err = tinyusb_driver_uninstall();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "TinyUSB uninstall failed: 0x%x", err);
    } else {
        s_installed = false;
        ESP_LOGI(TAG, "TinyUSB uninstalled successfully. Serial logs should resume.");
    }

    usb_serial_jtag_ll_phy_enable_external(false);

    vTaskDelay(pdMS_TO_TICKS(200));
}

void USBHIDKeyboard::sendReport(KeyReport* keys) {
    if (!s_report_sem || !s_report_mutex) return;
    if (!tud_hid_ready()) return;

    if (xSemaphoreTake(s_report_mutex, pdMS_TO_TICKS(100)) != pdTRUE) return;

    xSemaphoreTake(s_report_sem, 0);

    hid_keyboard_report_t report;
    report.reserved = 0;
    report.modifier = keys->modifiers;
    memcpy(report.keycode, keys->keys, 6);

    bool res = tud_hid_keyboard_report(HID_REPORT_ID_KEYBOARD, report.modifier, report.keycode);
    if (res) {

        xSemaphoreTake(s_report_sem, pdMS_TO_TICKS(100));
    }

    xSemaphoreGive(s_report_mutex);
}

size_t USBHIDKeyboard::pressRaw(uint8_t k) {
    if (k >= 0xE0 && k < 0xE8) {
        _keyReport.modifiers |= (1 << (k - 0xE0));
    } else if (k && k < 0xA5) {
        int8_t emptySlot = -1;
        for (uint8_t i = 0; i < 6; i++) {
            if (_keySlotMap[i] == k) return 1;
            if (emptySlot == -1 && _keyReport.keys[i] == 0x00) emptySlot = i;
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
    if (k >= 0xE0 && k < 0xE8) {
        _keyReport.modifiers &= ~(1 << (k - 0xE0));
    } else if (k && k < 0xA5) {
        for (uint8_t i = 0; i < 6; i++) {
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
        if (!_cacheValid) _buildShiftCache();
        if (k >= 128) return 0;

        k = _shiftCache[k];
        if (!k) return 0;
        if (k & SHIFT) {
            if (shiftKeyReports) pressRaw(HID_KEY_SHIFT_LEFT);
            else _keyReport.modifiers |= 0x02;
            k &= ~SHIFT;
        }
        if (k & ALT_GR) {
            _keyReport.modifiers |= 0x40;
            k &= ~ALT_GR;
        }
        if (k == ISO_REPLACEMENT) k = ISO_KEY;
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
        if (!_cacheValid) _buildShiftCache();
        if (k >= 128) return 0;

        k = _shiftCache[k];
        if (!k) return 0;
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
        if (k == ISO_REPLACEMENT) k = ISO_KEY;
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
    vTaskDelay(pdMS_TO_TICKS(_delay_ms ? _delay_ms : 10));
    release(c);
    vTaskDelay(pdMS_TO_TICKS(_delay_ms ? _delay_ms : 10));
    return p;
}

size_t USBHIDKeyboard::write(const uint8_t* buffer, size_t size) {
    size_t n = 0;
    while (size--) {
        if (*buffer != '\r') {
            if (write(*buffer)) n++;
            else break;
        }
        buffer++;
    }
    return n;
}

void USBHIDKeyboard::_buildShiftCache() {
    if (_asciimap == NULL) return;
    for (uint16_t i = 0; i < 128; i++) {
        _shiftCache[i] = _asciimap[i];
    }
    _cacheValid = true;
}

void USBHIDKeyboard::setLayoutByName(const std::string& lang_name) {
    const auto& layouts = get_keyboard_layouts();
    auto it = layouts.find(lang_name);
    if (it != layouts.end()) {
        setLayout(it->second);
        ESP_LOGI(TAG, "Keyboard layout set to: %s", lang_name.c_str());
    } else {
        ESP_LOGW(TAG, "Unknown layout: %s. Using en_US.", lang_name.c_str());
        setLayout(KeyboardLayout_en_US);
    }
}