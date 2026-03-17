#include "BleKeyboard.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_mac.h"
#include <NimBLEDevice.h>

#if defined(CONFIG_BT_ENABLED)

static const char *LOG_TAG = "BleKeyboard";

// We use the same ASCII to Keycode map as DuckyParser
extern const uint8_t _asciimap[];

#define SHIFT 0x80
#define ALT_GR 0xc0
#define ISO_REPLACEMENT 0x32
#define ISO_KEY 0x64

// Report IDs:
#define KEYBOARD_ID 0x01
#define MEDIA_KEYS_ID 0x02

static const uint8_t _hidReportDescriptor[] = {
    0x05, 0x01,        // USAGE_PAGE (Generic Desktop Ctrls)
    0x09, 0x06,        // USAGE (Keyboard)
    0xA1, 0x01,        // COLLECTION (Application)
    
    // ------------------------------------------------- Keyboard
    0x85, KEYBOARD_ID, //   REPORT_ID (1)
    0x05, 0x07,        //   USAGE_PAGE (Kbrd/Keypad)
    0x19, 0xE0,        //   USAGE_MINIMUM (0xE0)
    0x29, 0xE7,        //   USAGE_MAXIMUM (0xE7)
    0x15, 0x00,        //   LOGICAL_MINIMUM (0)
    0x25, 0x01,        //   LOGICAL_MAXIMUM (1)
    0x75, 0x01,        //   REPORT_SIZE (1)
    0x95, 0x08,        //   REPORT_COUNT (8)
    0x81, 0x02,        //   INPUT (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
    
    0x95, 0x01,        //   REPORT_COUNT (1) ; 1 byte (Reserved)
    0x75, 0x08,        //   REPORT_SIZE (8)
    0x81, 0x01,        //   INPUT (Const,Array,Abs,No Wrap,Linear,Preferred State,No Null Position)
    
    0x95, 0x05,        //   REPORT_COUNT (5) ; 5 bits (Num lock, Caps lock, Scroll lock, Compose, Kana)
    0x75, 0x01,        //   REPORT_SIZE (1)
    0x05, 0x08,        //   USAGE_PAGE (LEDs)
    0x19, 0x01,        //   USAGE_MINIMUM (0x01) ; Num Lock
    0x29, 0x05,        //   USAGE_MAXIMUM (0x05) ; Kana
    0x91, 0x02,        //   OUTPUT (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position,Non-volatile)
    
    0x95, 0x01,        //   REPORT_COUNT (1) ; 3 bits (Padding)
    0x75, 0x03,        //   REPORT_SIZE (3)
    0x91, 0x01,        //   OUTPUT (Const,Array,Abs,No Wrap,Linear,Preferred State,No Null Position,Non-volatile)
    
    0x95, 0x06,        //   REPORT_COUNT (6) ; 6 bytes (Keys)
    0x75, 0x08,        //   REPORT_SIZE(8)
    0x15, 0x00,        //   LOGICAL_MINIMUM(0)
    0x25, 0x65,        //   LOGICAL_MAXIMUM(0x65) ; 101 keys
    0x05, 0x07,        //   USAGE_PAGE (Kbrd/Keypad)
    0x19, 0x00,        //   USAGE_MINIMUM (0)
    0x29, 0x65,        //   USAGE_MAXIMUM (0x65)
    0x81, 0x00,        //   INPUT (Data,Array,Abs,No Wrap,Linear,Preferred State,No Null Position)
    0xC0,              // END_COLLECTION
    
    // ------------------------------------------------- Media Keys
    0x05, 0x0C,        // USAGE_PAGE (Consumer)
    0x09, 0x01,        // USAGE (Consumer Control)
    0xA1, 0x01,        // COLLECTION (Application)
    0x85, MEDIA_KEYS_ID, //   REPORT_ID (2)
    0x05, 0x0C,        //   USAGE_PAGE (Consumer)
    0x15, 0x00,        //   LOGICAL_MINIMUM (0)
    0x25, 0x01,        //   LOGICAL_MAXIMUM (1)
    0x75, 0x01,        //   REPORT_SIZE (1)
    0x95, 0x10,        //   REPORT_COUNT (16)
    0x09, 0xB5,        //   USAGE (Scan Next Track)     ; bit 0: 1
    0x09, 0xB6,        //   USAGE (Scan Previous Track) ; bit 1: 2
    0x09, 0xB7,        //   USAGE (Stop)                ; bit 2: 4
    0x09, 0xCD,        //   USAGE (Play/Pause)          ; bit 3: 8
    0x09, 0xE2,        //   USAGE (Mute)                ; bit 4: 16
    0x09, 0xE9,        //   USAGE (Volume Increment)    ; bit 5: 32
    0x09, 0xEA,        //   USAGE (Volume Decrement)    ; bit 6: 64
    0x0A, 0x23, 0x02,  //   Usage (WWW Home)            ; bit 7: 128
    0x0A, 0x94, 0x01,  //   Usage (My Computer) ; bit 0: 1
    0x0A, 0x92, 0x01,  //   Usage (Calculator)  ; bit 1: 2
    0x0A, 0x2A, 0x02,  //   Usage (WWW fav)     ; bit 2: 4
    0x0A, 0x21, 0x02,  //   Usage (WWW search)  ; bit 3: 8
    0x0A, 0x26, 0x02,  //   Usage (WWW stop)    ; bit 4: 16
    0x0A, 0x24, 0x02,  //   Usage (WWW back)    ; bit 5: 32
    0x0A, 0x83, 0x01,  //   Usage (Media sel)   ; bit 6: 64
    0x0A, 0x8A, 0x01,  //   Usage (Mail)        ; bit 7: 128
    0x81, 0x02,        //   INPUT (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
    0xC0               // END_COLLECTION
};

BleKeyboard::BleKeyboard(std::string deviceName, std::string deviceManufacturer, uint8_t batteryLevel)
    : hid(0), deviceName(deviceName.substr(0, 15)),
      deviceManufacturer(deviceManufacturer.substr(0, 15)), batteryLevel(batteryLevel) {
    // Basic init of structures
    _keyReport.keys[0] = 0;
    _keyReport.keys[1] = 0;
    _keyReport.keys[2] = 0;
    _keyReport.keys[3] = 0;
    _keyReport.keys[4] = 0;
    _keyReport.keys[5] = 0;
    _keyReport.modifiers = 0;
    _mediaKeyReport[0] = 0;
    _mediaKeyReport[1] = 0;
}

BleKeyboard::~BleKeyboard() {
    end();
}

void BleKeyboard::begin(const uint8_t *layout) {
    _asciimap = layout ? layout : KeyboardLayout_en_US; 
    
    NimBLEDevice::init(deviceName.c_str());
    pServer = NimBLEDevice::createServer();
    pServer->setCallbacks(new ServerCallbacks(this));
    hid = new NimBLEHIDDevice(pServer);
    
    inputKeyboard = hid->getInputReport(KEYBOARD_ID);
    outputKeyboard = hid->getOutputReport(KEYBOARD_ID);
    inputMediaKeys = hid->getInputReport(MEDIA_KEYS_ID);

    inputKeyboard->setCallbacks(new CharacteristicCallbacks(this));
    outputKeyboard->setCallbacks(new CharacteristicCallbacks(this));
    inputMediaKeys->setCallbacks(new CharacteristicCallbacks(this));

    hid->setManufacturer("Espressif");
    hid->setPnp(0x02, vid, pid, version);
    hid->setHidInfo(0x00, 0x01);

    NimBLEDevice::setSecurityAuth(true, true, true);

    hid->setReportMap((uint8_t *)_hidReportDescriptor, sizeof(_hidReportDescriptor));
    hid->startServices();
    advertising = pServer->getAdvertising();
    advertising->setAppearance(appearance);
    
    advertising->addServiceUUID(hid->getHidService()->getUUID());
    NimBLEAdvertisementData advertisementData = NimBLEAdvertisementData();
    advertisementData.setFlags(BLE_HS_ADV_F_DISC_GEN);
    advertisementData.setName(deviceName.c_str());
    advertisementData.setAppearance(appearance);
    advertising->setAdvertisementData(advertisementData);

    advertising->enableScanResponse(false);
    advertising->start();
    hid->setBatteryLevel(batteryLevel);
}

void BleKeyboard::end(void) {
    if (pServer != nullptr) {
        auto connectedPeers = pServer->getPeerDevices();
        for (auto &peer : connectedPeers) {
            ESP_LOGI(LOG_TAG, "Disconnecting peer...");
            pServer->disconnect(peer);
        }
    }
    
    if (hid != nullptr) {
        delete hid;
        hid = nullptr;
    }
    NimBLEDevice::deinit();
    this->connected = false;
}

bool BleKeyboard::isConnected(void) { return this->connected; }

void BleKeyboard::setBatteryLevel(uint8_t level) {
    this->batteryLevel = level;
    if (hid != nullptr) this->hid->setBatteryLevel(this->batteryLevel);
}

void BleKeyboard::setName(std::string deviceName) { this->deviceName = deviceName; }

void BleKeyboard::setDelay(uint32_t ms) { this->_delay_ms = ms; }

void BleKeyboard::set_vendor_id(uint16_t vid) { this->vid = vid; }

void BleKeyboard::set_product_id(uint16_t pid) { this->pid = pid; }

void BleKeyboard::set_version(uint16_t version) { this->version = version; }

void BleKeyboard::sendReport(KeyReport *keys) {
    if (this->isConnected() && this->getSubscribedCount() > 0) {
        this->inputKeyboard->setValue((uint8_t *)keys, sizeof(KeyReport));
        this->inputKeyboard->notify();
        this->delay_ms(_delay_ms);
    }
}

void BleKeyboard::sendReport(MediaKeyReport *keys) {
    if (this->isConnected() && this->getSubscribedCount() > 0) {
        this->inputMediaKeys->setValue((uint8_t *)keys, sizeof(MediaKeyReport));
        this->inputMediaKeys->notify();
        this->delay_ms(_delay_ms);
    }
}

size_t BleKeyboard::press(uint8_t k) {
    uint8_t i;
    if (k >= 0xE0 && k < 0xE8) {
        // k is not to be changed
    } else if (k >= 0x88) { // it's a non-printing key (not a modifier)
        k = k - 0x88;
    } else if (k >= 0x80) { // it's a modifier key
        _keyReport.modifiers |= (1 << (k - 0x80));
        k = 0;
    } else { // it's a printing key
        k = _asciimap[k];
        if (!k) {
            return 0;
        }
        if ((k & 0xc0) == 0xc0) {         // ALT_GR
            _keyReport.modifiers |= 0x40; // AltGr = right Alt
            k &= 0x3F;
        } else if ((k & 0x80) == 0x80) {  // SHIFT
            _keyReport.modifiers |= 0x02; // the left shift modifier
            k &= 0x7F;
        }
        if (k == 0x32) // ISO_REPLACEMENT
            k = 0x64;  // ISO_KEY
    }

    // Add k to the key report only if it's not already present
    // and if there is an empty slot.
    if (_keyReport.keys[0] != k && _keyReport.keys[1] != k && _keyReport.keys[2] != k &&
        _keyReport.keys[3] != k && _keyReport.keys[4] != k && _keyReport.keys[5] != k) {

        for (i = 0; i < 6; i++) {
            if (_keyReport.keys[i] == 0x00) {
                _keyReport.keys[i] = k;
                break;
            }
        }
        if (i == 6) {
            return 0;
        }
    }
    sendReport(&_keyReport);
    return 1;
}

size_t BleKeyboard::press(const MediaKeyReport k) {
    uint16_t k_16 = k[1] | (k[0] << 8);
    uint16_t mediaKeyReport_16 = _mediaKeyReport[1] | (_mediaKeyReport[0] << 8);

    mediaKeyReport_16 |= k_16;
    _mediaKeyReport[0] = (uint8_t)((mediaKeyReport_16 & 0xFF00) >> 8);
    _mediaKeyReport[1] = (uint8_t)(mediaKeyReport_16 & 0x00FF);

    sendReport(&_mediaKeyReport);
    return 1;
}

size_t BleKeyboard::release(uint8_t k) {
    uint8_t i;
    if (k >= 136) { // it's a non-printing key (not a modifier)
        k = k - 136;
    } else if (k >= 128) { // it's a modifier key
        _keyReport.modifiers &= ~(1 << (k - 128));
        k = 0;
    } else { // it's a printing key
        k = _asciimap[k];
        if (!k) { return 0; }
        if ((k & ALT_GR) == ALT_GR) {
            _keyReport.modifiers &= ~(0x40); // AltGr = right Alt
            k &= 0x3F;
        } else if ((k & SHIFT) == SHIFT) {
            _keyReport.modifiers &= ~(0x02); // the left shift modifier
            k &= 0x7F;
        }
        if (k == ISO_REPLACEMENT) { k = ISO_KEY; }
    }

    for (i = 0; i < 6; i++) {
        if (0 != k && _keyReport.keys[i] == k) { _keyReport.keys[i] = 0x00; }
    }

    sendReport(&_keyReport);
    return 1;
}

size_t BleKeyboard::release(const MediaKeyReport k) {
    uint16_t k_16 = k[1] | (k[0] << 8);
    uint16_t mediaKeyReport_16 = _mediaKeyReport[1] | (_mediaKeyReport[0] << 8);
    mediaKeyReport_16 &= ~k_16;
    _mediaKeyReport[0] = (uint8_t)((mediaKeyReport_16 & 0xFF00) >> 8);
    _mediaKeyReport[1] = (uint8_t)(mediaKeyReport_16 & 0x00FF);

    sendReport(&_mediaKeyReport);
    return 1;
}

void BleKeyboard::releaseAll(void) {
    _keyReport.keys[0] = 0;
    _keyReport.keys[1] = 0;
    _keyReport.keys[2] = 0;
    _keyReport.keys[3] = 0;
    _keyReport.keys[4] = 0;
    _keyReport.keys[5] = 0;
    _keyReport.modifiers = 0;
    _mediaKeyReport[0] = 0;
    _mediaKeyReport[1] = 0;
    sendReport(&_keyReport);
    sendReport(&_mediaKeyReport);
}

size_t BleKeyboard::write(uint8_t c) {
    uint8_t p = press(c); // Keydown
    release(c);           // Keyup
    return p;
}

size_t BleKeyboard::write(const MediaKeyReport c) {
    uint16_t p = press(c); // Keydown
    release(c);            // Keyup
    return p;
}

size_t BleKeyboard::write(const uint8_t *buffer, size_t size) {
    size_t n = 0;
    // C pointer cast instead of Arduino while
    for(size_t i = 0; i < size; i++) {
        if (buffer[i] != '\r') {
            if (write(buffer[i])) {
                n++;
            } else {
                break;
            }
        }
    }
    return n;
}

void BleKeyboard::ServerCallbacks::onConnect(NimBLEServer *pServer, NimBLEConnInfo &connInfo) {
    ESP_LOGI(LOG_TAG, "lib connected");
}
void BleKeyboard::ServerCallbacks::onDisconnect(NimBLEServer *pServer, NimBLEConnInfo &connInfo, int reason) {
    ESP_LOGI(LOG_TAG, "lib disconnected");
    parent->connected = false;
}
void BleKeyboard::ServerCallbacks::onAuthenticationComplete(NimBLEConnInfo &connInfo) {
    if (connInfo.isEncrypted()) {
        ESP_LOGI(LOG_TAG, "Paired successfully.");
        parent->connected = true;
    } else {
        ESP_LOGW(LOG_TAG, "Pairing failed");
        parent->connected = false;
    }
}

void BleKeyboard::CharacteristicCallbacks::onWrite(
    NimBLECharacteristic *pCharacteristic, NimBLEConnInfo &connInfo
) {
    std::string val = pCharacteristic->getValue();
    if(val.length() > 0) {
        ESP_LOGI(LOG_TAG, "special keys: %d", val[0]);
    }
}
void BleKeyboard::CharacteristicCallbacks::onSubscribe(
    NimBLECharacteristic *pCharacteristic, NimBLEConnInfo &connInfo, uint16_t subValue
) {
    if (subValue == 0) {
        ESP_LOGI(LOG_TAG, "Client unsubscribed from notifications.");
        if (parent->m_subCount) parent->m_subCount--;
    } else {
        parent->m_subCount++;
        ESP_LOGI(LOG_TAG, "Client subscribed to notifications.");
    }
}

void BleKeyboard::delay_ms(uint64_t ms) {
    vTaskDelay(pdMS_TO_TICKS(ms));
}

#endif // CONFIG_BT_ENABLED
