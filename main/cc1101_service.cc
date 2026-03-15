#include "cc1101_service.h"
#include <cJSON.h>
#include <dirent.h>
#include <unistd.h>
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <sstream>
#include <string>
#include "boards/t-embed/config.h"
#include "driver/rmt_tx.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define TAG "Cc1101"

#ifndef SDCARD_MOUNT_POINT
#define SDCARD_MOUNT_POINT "/sdcard"
#endif

// ---------------------------------------------------------------------------
// CC1101 command strobes
// ---------------------------------------------------------------------------
#define CC1101_SRES    0x30
#define CC1101_SRX     0x34
#define CC1101_STX     0x35
#define CC1101_SIDLE   0x36
#define CC1101_SFRX    0x3A
#define CC1101_SFTX    0x3B
#define CC1101_VERSION 0x31
#define CC1101_DEVIATN 0x15
#define CC1101_PKTCTRL0 0x08
#define CC1101_FREND0  0x22
#define CC1101_RSSI    0x34

// ---------------------------------------------------------------------------
// Hardcoded Tesla Port Opener Signal (433.92 MHz, ASK/OOK, AM650)
// Extracted from Tesla_EU_AM650.sub
// ---------------------------------------------------------------------------
static const std::vector<int32_t> TESLA_AM650_RAW = {
    400, -400, 400, -400, 400, -400, 400, -400, 400, -400, 400, -400, 400, -400, 400, -400, 400, -400, 400, -400, 
    400, -400, 400, -400, 400, -1200, 400, -400, 400, -400, 800, -800, 400, -400, 800, -800, 800, -800, 400, -400, 
    800, -800, 800, -800, 800, -800, 800, -800, 800, -800, 400, -400, 800, -400, 400, -800, 800, -400, 400, -800, 
    400, -400, 800, -400, 400, -400, 400, -800, 400, -400, 400, -400, 800, -400, 400, -800, 800, -400, 400, -800, 
    800, -800, 400, -400, 400, -400, 400, -400, 800, -400, 400, -800, 400, -400, 800, -1200, 400, -400, 400, -400, 
    800, -800, 400, -400, 800, -800, 800, -800, 400, -400, 800, -800, 800, -800, 800, -800, 800, -800, 800, -800, 
    400, -400, 800, -400, 400, -800, 800, -400, 400, -800, 400, -400, 800, -400, 400, -400, 400, -800, 400, -400, 
    400, -400, 800, -400, 400, -800, 800, -400, 400, -800, 800, -800, 400, -400, 400, -400, 400, -400, 800, -400, 
    400, -800, 400, -400, 800, -1200, 400, -400, 400, -400, 800, -800, 400, -400, 800, -800, 800, -800, 400, -400, 
    800, -800, 800, -800, 800, -800, 800, -800, 800, -800, 400, -400, 800, -400, 400, -800, 800, -400, 400, -800, 
    400, -400, 800, -400, 400, -400, 400, -800, 400, -400, 400, -400, 800, -400, 400, -800, 800, -400, 400, -800, 
    800, -800, 400, -400, 400, -400, 400, -400, 800, -400, 400, -800, 400, -400, 400, -25000
};

// ---------------------------------------------------------------------------
// Flipper Zero compatible register sets (mirrored from Bruce fw / Flipper src)
// ---------------------------------------------------------------------------
static const Cc1101RegisterSetting kPresetOok270[] = {
    {0x00, 0x29}, {0x0B, 0x06}, {0x0C, 0x00}, {0x10, 0xC0}, {0x11, 0x00},
    {0x12, 0x30}, {0x13, 0x00}, {0x14, 0x00}, {0x15, 0x00}, {0x17, 0x30},
    {0x18, 0x18}, {0x19, 0x18}, {0x1A, 0x6C}, {0x1B, 0x43}, {0x1C, 0x40},
    {0x1D, 0x91}, {0x21, 0x56}, {0x22, 0x10}, {0x23, 0xE9}, {0x24, 0x2A},
    {0x25, 0x00}, {0x26, 0x1F}, {0x08, 0x32}};

static const Cc1101RegisterSetting kPresetOok650[] = {
    {0x00, 0x29}, {0x0B, 0x06}, {0x0C, 0x00}, {0x10, 0xC8}, {0x11, 0x93},
    {0x12, 0x30}, {0x13, 0x22}, {0x14, 0xF8}, {0x15, 0x34}, {0x17, 0x30},
    {0x18, 0x18}, {0x19, 0x16}, {0x1A, 0x6C}, {0x1B, 0x43}, {0x1C, 0x40},
    {0x1D, 0x91}, {0x21, 0x56}, {0x22, 0x10}, {0x23, 0xE9}, {0x24, 0x2A},
    {0x25, 0x00}, {0x26, 0x1F}, {0x08, 0x32}};

static const Cc1101RegisterSetting kPreset2FSK238[] = {
    {0x00, 0x29}, {0x0B, 0x06}, {0x0C, 0x00}, {0x10, 0xC7}, {0x11, 0x32},
    {0x12, 0x06}, {0x13, 0x22}, {0x14, 0xF5}, {0x15, 0x00}, {0x17, 0x30},
    {0x18, 0x18}, {0x19, 0x18}, {0x1A, 0x6C}, {0x1B, 0x43}, {0x1C, 0x40},
    {0x1D, 0x91}, {0x21, 0x56}, {0x22, 0x10}, {0x23, 0xE9}, {0x24, 0x2A},
    {0x25, 0x00}, {0x26, 0x1F}, {0x08, 0x00}};

static const Cc1101RegisterSetting kPreset2FSK476[] = {
    {0x00, 0x29}, {0x0B, 0x06}, {0x0C, 0x00}, {0x10, 0xC7}, {0x11, 0x32},
    {0x12, 0x06}, {0x13, 0x22}, {0x14, 0xF5}, {0x15, 0x50}, {0x17, 0x30},
    {0x18, 0x18}, {0x19, 0x18}, {0x1A, 0x6C}, {0x1B, 0x43}, {0x1C, 0x40},
    {0x1D, 0x91}, {0x21, 0x56}, {0x22, 0x10}, {0x23, 0xE9}, {0x24, 0x2A},
    {0x25, 0x00}, {0x26, 0x1F}, {0x08, 0x00}};

static const Cc1101RegisterSetting kPresetMSK[] = {
    {0x00, 0x29}, {0x0B, 0x06}, {0x0C, 0x00}, {0x10, 0x24}, {0x11, 0x24},
    {0x12, 0x73}, {0x13, 0x22}, {0x14, 0xF8}, {0x15, 0x63}, {0x17, 0x30},
    {0x18, 0x18}, {0x19, 0x18}, {0x1A, 0x6C}, {0x1B, 0x43}, {0x1C, 0x40},
    {0x1D, 0x91}, {0x21, 0x56}, {0x22, 0x10}, {0x23, 0xE9}, {0x24, 0x2A},
    {0x25, 0x00}, {0x26, 0x1F}, {0x08, 0x00}};

static const Cc1101RegisterSetting kPresetGFSK[] = {
    {0x00, 0x29}, {0x0B, 0x06}, {0x0C, 0x00}, {0x10, 0xC7}, {0x11, 0x83},
    {0x12, 0x12}, {0x13, 0x22}, {0x14, 0xF8}, {0x15, 0x4E}, {0x17, 0x30},
    {0x18, 0x18}, {0x19, 0x18}, {0x1A, 0x6C}, {0x1B, 0x43}, {0x1C, 0x40},
    {0x1D, 0x91}, {0x21, 0x56}, {0x22, 0x10}, {0x23, 0xE9}, {0x24, 0x2A},
    {0x25, 0x00}, {0x26, 0x1F}, {0x08, 0x00}};

// ---------------------------------------------------------------------------
// Constructor / Destructor
// ---------------------------------------------------------------------------
Cc1101Service::Cc1101Service() {}

Cc1101Service::~Cc1101Service() {
    if (rmt_encoder_) rmt_del_encoder(rmt_encoder_);
    if (rmt_tx_channel_) rmt_del_channel(rmt_tx_channel_);
    if (spi_handle_) spi_bus_remove_device(spi_handle_);
}

// ---------------------------------------------------------------------------
// Low-level SPI helpers
// ---------------------------------------------------------------------------
void Cc1101Service::CommandStrobe(uint8_t cmd) {
    if (!spi_handle_) return;
    std::lock_guard<std::mutex> lock(spi_mutex_);
    gpio_set_level(LORA_CS_PIN, 0);
    spi_transaction_t t = {};
    t.length = 8;
    t.flags = SPI_TRANS_USE_TXDATA;
    t.tx_data[0] = cmd;
    spi_device_transmit(spi_handle_, &t);
    gpio_set_level(LORA_CS_PIN, 1);
}

void Cc1101Service::_CommandStrobe(uint8_t cmd) { CommandStrobe(cmd); }

void Cc1101Service::WriteReg(uint8_t addr, uint8_t data) {
    if (!spi_handle_) return;
    std::lock_guard<std::mutex> lock(spi_mutex_);
    gpio_set_level(LORA_CS_PIN, 0);
    spi_transaction_t t = {};
    t.length = 16;
    t.flags = SPI_TRANS_USE_TXDATA;
    t.tx_data[0] = addr;
    t.tx_data[1] = data;
    spi_device_transmit(spi_handle_, &t);
    gpio_set_level(LORA_CS_PIN, 1);
}

void Cc1101Service::WriteBurstReg(uint8_t addr, const uint8_t* buffer, uint8_t num) {
    if (!spi_handle_ || !buffer || num == 0) return;
    std::lock_guard<std::mutex> lock(spi_mutex_);
    
    // Convert register address to write burst access (add 0x40 flag)
    uint8_t burst_addr = addr | 0x40;
    
    // We max out at 8 bytes for PATABLE typically
    uint8_t tx_buffer[9];
    tx_buffer[0] = burst_addr;
    memcpy(&tx_buffer[1], buffer, (num > 8 ? 8 : num));
    
    gpio_set_level(LORA_CS_PIN, 0);
    spi_transaction_t t = {};
    t.length = (1 + (num > 8 ? 8 : num)) * 8; // 1 byte addr + num bytes data
    t.tx_buffer = tx_buffer;
    spi_device_transmit(spi_handle_, &t);
    gpio_set_level(LORA_CS_PIN, 1);
}

uint8_t Cc1101Service::ReadReg(uint8_t addr) {
    if (!spi_handle_) return 0;
    std::lock_guard<std::mutex> lock(spi_mutex_);
    gpio_set_level(LORA_CS_PIN, 0);
    spi_transaction_t t = {};
    t.length = 16;
    t.flags = SPI_TRANS_USE_TXDATA | SPI_TRANS_USE_RXDATA;
    t.tx_data[0] = (uint8_t)(addr | 0x80);
    t.tx_data[1] = 0;
    spi_device_transmit(spi_handle_, &t);
    gpio_set_level(LORA_CS_PIN, 1);
    return t.rx_data[1];
}

void Cc1101Service::Reset() {
    std::lock_guard<std::mutex> lock(spi_mutex_);
    gpio_set_level(LORA_CS_PIN, 1);
    vTaskDelay(pdMS_TO_TICKS(1));
    gpio_set_level(LORA_CS_PIN, 0);
    vTaskDelay(pdMS_TO_TICKS(1));
    gpio_set_level(LORA_CS_PIN, 1);
    vTaskDelay(pdMS_TO_TICKS(41));
    gpio_set_level(LORA_CS_PIN, 0);
    vTaskDelay(pdMS_TO_TICKS(10));
    
    // Manual strobe inside Reset (mutex already locked)
    spi_transaction_t t = {};
    t.length = 8;
    t.flags = SPI_TRANS_USE_TXDATA;
    t.tx_data[0] = CC1101_SRES;
    spi_device_transmit(spi_handle_, &t);

    vTaskDelay(pdMS_TO_TICKS(10));
    gpio_set_level(LORA_CS_PIN, 1);
}

// ---------------------------------------------------------------------------
// Init / Deinit
// ---------------------------------------------------------------------------
bool Cc1101Service::Deinit() {
    if (!initialized_) return true;
    if (spi_handle_) {
        esp_err_t ret = spi_bus_remove_device(spi_handle_);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to remove SPI device: %s", esp_err_to_name(ret));
            return false;
        }
        spi_handle_ = nullptr;
    }
    initialized_ = false;
    ESP_LOGI(TAG, "CC1101 SPI device removed.");
    return true;
}

bool Cc1101Service::Init() {
    if (initialized_) return true;
    spi_device_interface_config_t devcfg = {};
    devcfg.clock_speed_hz = 5 * 1000 * 1000;
    devcfg.mode = 0;
    devcfg.spics_io_num = -1;
    devcfg.queue_size = 7;
    esp_err_t ret = spi_bus_add_device(SPI2_HOST, &devcfg, &spi_handle_);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to add SPI device: %s", esp_err_to_name(ret));
        return false;
    }
    Reset();
    WriteReg(CC1101_MDMCFG4, 0x8B);
    WriteReg(CC1101_MDMCFG3, 0xF8);
    WriteReg(CC1101_MDMCFG2, 0x30);
    WriteReg(CC1101_DEVIATN, 0x47);
    WriteReg(0x17, 0x00);
    WriteReg(0x18, 0x18);
    WriteReg(0x1D, 0x91);
    WriteReg(0x1E, 0x87);
    WriteReg(0x0B, 0x06);
    WriteReg(CC1101_PKTCTRL0, 0x30);
    gpio_set_direction((gpio_num_t)LORA_SW1_PIN, GPIO_MODE_OUTPUT);
    gpio_set_direction((gpio_num_t)LORA_SW0_PIN, GPIO_MODE_OUTPUT);
    SetRfSettings();
    uint8_t version = ReadReg(CC1101_VERSION | 0xC0);
    ESP_LOGI(TAG, "CC1101 version: 0x%02X", version);
    if (version == 0x00 || version == 0xFF) {
        ESP_LOGE(TAG, "CC1101 SPI error! Version: 0x%02X", version);
    }
    initialized_ = true;
    ESP_LOGI(TAG, "CC1101 ready.");
    return true;
}

void Cc1101Service::SetRfSettings() { SetFrequency(433.92f); }

// ---------------------------------------------------------------------------
// Frequency / Modulation
// ---------------------------------------------------------------------------
bool Cc1101Service::SetFrequency(float mhz) {
    if (!initialized_) return false;
    if (mhz <= 350.0f) {
        gpio_set_level(LORA_SW1_PIN, 1);
        gpio_set_level(LORA_SW0_PIN, 0);
    } else if (mhz > 351.0f && mhz < 468.0f) {
        gpio_set_level(LORA_SW1_PIN, 1);
        gpio_set_level(LORA_SW0_PIN, 1);
    } else if (mhz >= 779.0f) {
        gpio_set_level(LORA_SW1_PIN, 0);
        gpio_set_level(LORA_SW0_PIN, 1);
    }
    vTaskDelay(pdMS_TO_TICKS(10));
    uint32_t freq = (uint32_t)((mhz * 65536.0) / 26.0);
    WriteReg(0x0D, (freq >> 16) & 0xFF);
    WriteReg(0x0E, (freq >> 8) & 0xFF);
    WriteReg(0x0F, freq & 0xFF);
    current_frequency_mhz_ = mhz;
    ESP_LOGI(TAG, "Frequency: %.2f MHz", mhz);
    return true;
}

bool Cc1101Service::SetModulation(const std::string& type) {
    if (!initialized_) return false;
    if (presets_.count(type)) return ApplyPreset(type);
    if (type == "ASK" || type == "OOK" || type == "AM") {
        for (const auto& r : kPresetOok650) WriteReg(r.reg, r.val);
        current_modulation_ = "AM (ASK)";
        ESP_LOGI(TAG, "ASK/OOK modu uygulandi");
    } else {
        WriteReg(0x10, 0x2D);
        WriteReg(0x11, 0x3B);
        WriteReg(0x12, 0x00);
        WriteReg(0x15, 0x62);
        current_modulation_ = "FM (FSK)";
        ESP_LOGI(TAG, "FSK modu uygulandi");
    }
    return true;
}

// ---------------------------------------------------------------------------
// Apply Flipper-compatible preset by name
// ---------------------------------------------------------------------------
bool Cc1101Service::ApplyFlipperPreset(const std::string& preset_name) {
    struct { const char* name; const Cc1101RegisterSetting* regs; size_t count; const char* mod; } table[] = {
        {"FuriHalSubGhzPresetOok270Async",    kPresetOok270,  sizeof(kPresetOok270)/sizeof(*kPresetOok270),  "AM (ASK)"},
        {"FuriHalSubGhzPresetOok650Async",    kPresetOok650,  sizeof(kPresetOok650)/sizeof(*kPresetOok650),  "AM (ASK)"},
        {"FuriHalSubGhzPreset2FSKDev238Async",kPreset2FSK238, sizeof(kPreset2FSK238)/sizeof(*kPreset2FSK238),"FM (FSK)"},
        {"FuriHalSubGhzPreset2FSKDev476Async",kPreset2FSK476, sizeof(kPreset2FSK476)/sizeof(*kPreset2FSK476),"FM (FSK)"},
        {"FuriHalSubGhzPresetMSK99_97KbAsync",kPresetMSK,     sizeof(kPresetMSK)/sizeof(*kPresetMSK),        "FM (MSK)"},
        {"FuriHalSubGhzPresetGFSK9_99KbAsync",kPresetGFSK,    sizeof(kPresetGFSK)/sizeof(*kPresetGFSK),      "FM (GFSK)"},
    };
    for (const auto& e : table) {
        if (preset_name == e.name) {
            for (size_t i = 0; i < e.count; i++) WriteReg(e.regs[i].reg, e.regs[i].val);
            current_modulation_ = e.mod;
            ESP_LOGI(TAG, "Flipper preset uygulandi: %s (%s)", e.name, e.mod);
            return true;
        }
    }
    ESP_LOGW(TAG, "Bilinmeyen Flipper preset: %s, OOK650 varsayilan.", preset_name.c_str());
    for (const auto& r : kPresetOok650) WriteReg(r.reg, r.val);
    current_modulation_ = "AM (ASK)";
    return false;
}

float Cc1101Service::ReadRssi() {
    if (!initialized_) return -100.0f;
    uint8_t state = ReadReg(0x35 | 0xC0) & 0x1F;
    if (state != 0x0D) {
        CommandStrobe(CC1101_SRX);
        vTaskDelay(pdMS_TO_TICKS(2));
    }
    uint8_t rssi_raw = ReadReg(0x34 | 0xC0);
    if (rssi_raw >= 128) return ((float)rssi_raw - 256) / 2.0f - 74.0f;
    return (float)rssi_raw / 2.0f - 74.0f;
}

bool Cc1101Service::GetStatus() { return initialized_; }
uint8_t Cc1101Service::GetChipVersion() { return ReadReg(CC1101_VERSION | 0xC0); }

void Cc1101Service::DumpRegisters() {
    ESP_LOGI(TAG, "--- CC1101 REGISTER DUMP ---");
    for (uint8_t i = 0; i <= 0x2E; i++) ESP_LOGI(TAG, "Reg 0x%02X: 0x%02X", i, ReadReg(i | 0xC0));
    ESP_LOGI(TAG, "----------------------------");
}

// ---------------------------------------------------------------------------
// JSON preset loading (custom user presets via SD card)
// ---------------------------------------------------------------------------
bool Cc1101Service::LoadPresets(const std::string& path) {
    FILE* f = fopen(path.c_str(), "r");
    if (!f) { ESP_LOGE(TAG, "Failed to open preset file: %s", path.c_str()); return false; }
    fseek(f, 0, SEEK_END); long size = ftell(f); fseek(f, 0, SEEK_SET);
    char* buffer = (char*)malloc(size + 1);
    if (!buffer) { fclose(f); return false; }
    fread(buffer, 1, size, f); buffer[size] = '\0'; fclose(f);
    cJSON* root = cJSON_Parse(buffer); free(buffer);
    if (!root) { ESP_LOGE(TAG, "JSON parse error: %s", path.c_str()); return false; }
    cJSON* arr = cJSON_GetObjectItem(root, "presets");
    if (cJSON_IsArray(arr)) {
        presets_.clear();
        int n = cJSON_GetArraySize(arr);
        for (int i = 0; i < n; i++) {
            cJSON* item = cJSON_GetArrayItem(arr, i);
            Cc1101Preset p;
            cJSON* name = cJSON_GetObjectItem(item, "name");
            cJSON* mod  = cJSON_GetObjectItem(item, "modulation");
            cJSON* freq = cJSON_GetObjectItem(item, "frequency");
            if (name) p.name       = name->valuestring;
            if (mod)  p.modulation = mod->valuestring;
            if (freq) p.frequency  = (float)freq->valuedouble;
            cJSON* regs = cJSON_GetObjectItem(item, "registers");
            if (cJSON_IsArray(regs)) {
                for (int j = 0; j < cJSON_GetArraySize(regs); j++) {
                    cJSON* ritem = cJSON_GetArrayItem(regs, j);
                    Cc1101RegisterSetting rs;
                    cJSON* r = cJSON_GetObjectItem(ritem, "reg");
                    cJSON* v = cJSON_GetObjectItem(ritem, "val");
                    if (r && v) { rs.reg = (uint8_t)strtol(r->valuestring, nullptr, 0); rs.val = (uint8_t)strtol(v->valuestring, nullptr, 0); p.registers.push_back(rs); }
                }
            }
            presets_[p.name] = p;
        }
    }
    cJSON_Delete(root);
    ESP_LOGI(TAG, "%zu preset yuklendi: %s", presets_.size(), path.c_str());
    return true;
}

bool Cc1101Service::ApplyPreset(const std::string& preset_name) {
    auto it = presets_.find(preset_name);
    if (it == presets_.end()) { ESP_LOGE(TAG, "Preset bulunamadi: %s", preset_name.c_str()); return false; }
    const auto& p = it->second;
    for (const auto& rs : p.registers) WriteReg(rs.reg, rs.val);
    current_modulation_ = p.modulation;
    ESP_LOGI(TAG, "Preset uygulandi: %s", preset_name.c_str());
    return true;
}

// ---------------------------------------------------------------------------
// SD Card file listing
// ---------------------------------------------------------------------------
std::string Cc1101Service::ListSdFiles() const {
    DIR* dir = opendir(SDCARD_MOUNT_POINT);
    if (!dir) return "Error: failed to open /sdcard";
    std::string result = "SD Card files (/sdcard):\n";
    struct dirent* entry; int count = 0;
    while ((entry = readdir(dir)) != NULL) {
        std::string name = entry->d_name;
        if (name == "." || name == "..") continue;
        std::string lower = name;
        std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
        std::string label = (entry->d_type == DT_DIR) ? "[klasor]" :
                            (lower.rfind(".sub")  != lower.npos) ? "[sub]"  :
                            (lower.rfind(".json") != lower.npos) ? "[json]" :
                            (lower.rfind(".txt")  != lower.npos) ? "[txt]"  : "[file]";
        result += label + " " + name + "\n"; count++;
    }
    closedir(dir);
    if (count == 0) return "No files found on SD card.";
    result += "Total: " + std::to_string(count) + " files.";
    return result;
}

std::string Cc1101Service::ListSdSubFiles() const {
    DIR* dir = opendir(SDCARD_MOUNT_POINT);
    if (!dir) return "Error: failed to open /sdcard";
    std::string result = ".sub files:\n";
    struct dirent* entry; int count = 0;
    while ((entry = readdir(dir)) != NULL) {
        std::string name = entry->d_name;
        if (name == "." || name == "..") continue;
        if (entry->d_type == DT_DIR) continue;
        std::string lower = name;
        std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
        if (lower.rfind(".sub") != lower.npos) { result += name + "\n"; count++; }
    }
    closedir(dir);
    if (count == 0) return "No .sub files found on SD card.";
    return result;
}

// ---------------------------------------------------------------------------
// Case-insensitive file lookup (searches directory for .sub files)
// ---------------------------------------------------------------------------
std::string Cc1101Service::FindFileCaseInsensitive(const std::string& directory,
                                                   const std::string& filename) {
    DIR* dir = opendir(directory.c_str());
    if (!dir) return "";
    std::string search_name = filename;
    std::transform(search_name.begin(), search_name.end(), search_name.begin(), ::tolower);
    if (search_name.find('.') == std::string::npos) search_name += ".sub";
    struct dirent* de; std::string found_path;
    while ((de = readdir(dir)) != NULL) {
        std::string entry_name = de->d_name;
        if (entry_name == "." || entry_name == "..") continue;
        if (de->d_type == DT_DIR) continue;
        
        std::string entry_lower = entry_name;
        std::transform(entry_lower.begin(), entry_lower.end(), entry_lower.begin(), ::tolower);
        if (entry_lower == search_name) { found_path = directory + "/" + entry_name; break; }
    }
    closedir(dir);
    return found_path;
}

// ---------------------------------------------------------------------------
// .sub File Parser — Reads all Flipper-compatible fields
// ---------------------------------------------------------------------------
bool Cc1101Service::ParseSubFile(const std::string& path, SubFileData& out) {
    ESP_LOGI(TAG, "Opening file for parsing: %s", path.c_str());
    FILE* f = fopen(path.c_str(), "r");
    if (!f) { ESP_LOGE(TAG, "Cannot open: %s", path.c_str()); return false; }

    out = SubFileData{};  // reset

    char line[2048];
    bool in_raw = false;

    while (fgets(line, sizeof(line), f)) {
        size_t len = strlen(line);
        while (len > 0 && (line[len-1] == '\r' || line[len-1] == '\n')) line[--len] = '\0';
        ESP_LOGI(TAG, "Read line: %s", line);

        // ---- Frequency ----
        if (strncmp(line, "Frequency:", 10) == 0) {
            out.frequency = (float)atof(line + 10);
            in_raw = false;

        // ---- Preset ----
        } else if (strncmp(line, "Preset:", 7) == 0) {
            char* p = line + 7; while (*p == ' ') p++;
            out.preset = std::string(p);
            in_raw = false;

        // ---- Protocol ----
        } else if (strncmp(line, "Protocol:", 9) == 0) {
            char* p = line + 9; while (*p == ' ') p++;
            out.protocol = std::string(p);
            in_raw = false;

        // ---- TE (pulse width µs) ----
        } else if (strncmp(line, "TE:", 3) == 0) {
            out.te = atoi(line + 3);
            in_raw = false;

        // ---- Bit count ----
        } else if (strncmp(line, "Bit:", 4) == 0 || strncmp(line, "Bit_RAW:", 8) == 0) {
            char* p = strchr(line, ':') + 1;
            out.bit = atoi(p);
            in_raw = false;

        // ---- Key (hex) ----
        } else if (strncmp(line, "Key:", 4) == 0) {
            char* p = line + 4; while (*p == ' ') p++;
            out.key = (uint64_t)strtoull(p, nullptr, 16);
            in_raw = false;

        // ---- Data (binary string for BinRAW) ----
        } else if (strncmp(line, "Data:", 5) == 0) {
            char* p = line + 5; while (*p == ' ') p++;
            out.bin_data = std::string(p);
            in_raw = false;

        // ---- RAW_Data / Data_RAW — may span multiple lines ----
        } else if (strncmp(line, "RAW_Data:", 9) == 0 || strncmp(line, "Data_RAW:", 9) == 0) {
            in_raw = true;
            char* data_ptr = strchr(line, ':') + 1;
            char* token = strtok(data_ptr, " \t");
            while (token) {
                int32_t val = (int32_t)atol(token);
                if (val != 0) out.raw_samples.push_back(val);
                token = strtok(NULL, " \t");
            }

        // ---- continuation of RAW_Data (no colon in line) ----
        } else if (in_raw && len > 0 && strchr(line, ':') == NULL) {
            char* token = strtok(line, " \t");
            while (token) {
                int32_t val = (int32_t)atol(token);
                if (val != 0) out.raw_samples.push_back(val);
                token = strtok(NULL, " \t");
            }

        // ---- any other keyed line ends raw block ----
        } else if (in_raw && strchr(line, ':') != NULL) {
            in_raw = false;
        }
    }

    fclose(f);
    ESP_LOGI(TAG, "ParseSubFile: freq=%.0f protocol='%s' preset='%s' key=%llu bit=%d te=%d raw=%d",
             out.frequency, out.protocol.c_str(), out.preset.c_str(),
             (unsigned long long)out.key, out.bit, out.te, (int)out.raw_samples.size());
    return true;
}

// ---------------------------------------------------------------------------
// RMT-based RAW sample transmission (shared helper)
// ---------------------------------------------------------------------------
static bool rmt_transmit_raw(const std::vector<int32_t>& buf, uint8_t gdo0_pin,
                             Cc1101Service* svc) {
    rmt_tx_channel_config_t tx_chan_config = {
        .gpio_num          = (gpio_num_t)gdo0_pin,
        .clk_src           = RMT_CLK_SRC_DEFAULT,
        .resolution_hz     = 1000000,   // 1 tick = 1 µs
        .mem_block_symbols = 64,
        .trans_queue_depth = 4,
        .flags             = {.invert_out = false, .with_dma = false,
                              .io_loop_back = false, .io_od_mode = false}};
    rmt_channel_handle_t ch = nullptr;
    if (rmt_new_tx_channel(&tx_chan_config, &ch) != ESP_OK) {
        ESP_LOGE("Cc1101", "RMT TX channel create failed");
        return false;
    }
    rmt_copy_encoder_config_t enc_cfg = {};
    rmt_encoder_handle_t enc = nullptr;
    if (rmt_new_copy_encoder(&enc_cfg, &enc) != ESP_OK) {
        rmt_del_channel(ch); return false;
    }
    if (rmt_enable(ch) != ESP_OK) {
        rmt_del_encoder(enc); rmt_del_channel(ch); return false;
    }

    // Build RMT symbol table from signed timings
    std::vector<rmt_symbol_word_t> symbols;
    symbols.reserve(buf.size() / 2 + 1);
    rmt_symbol_word_t sym; bool hp = false;
    for (size_t i = 0; i < buf.size(); i++) {
        int32_t val = buf[i];
        uint32_t dur = (val > 0) ? (uint32_t)val : (uint32_t)(-val);
        if (dur > 32767) dur = 32767;
        if (val > 0) {
            sym.duration1 = dur; sym.level1 = 1; hp = true;
        } else {
            sym.duration0 = dur; sym.level0 = 0;
            if (!hp) { sym.duration1 = 1; sym.level1 = 1; }
            symbols.push_back(sym); hp = false;
        }
    }
    if (hp) { sym.duration0 = 1; sym.level0 = 0; symbols.push_back(sym); }

    if (!symbols.empty()) {
        rmt_transmit_config_t tx_cfg = {.loop_count = 0, .flags = {.eot_level = 0}};
        
        // --- CC1101 Initialization for RMT Transmission ---
        // 1. Enter IDLE mode before configuration
        svc->_CommandStrobe(CC1101_SIDLE);
        vTaskDelay(pdMS_TO_TICKS(5));

        // 2. Set Async Serial TX mode (PKT_FORMAT=11)
        svc->_WriteRegPub(CC1101_PKTCTRL0, 0x32);
        
        // 3. Disable CC1101's own GDO0 output driver (IOCFG0 = Hi-Z)
        svc->_WriteRegPub(0x02, 0x2E);

        // 4. Set TX power in PATABLE for ASK/OOK (0 dBm = 0x00, +10 dBm = 0xC0)
        uint8_t paTable[8] = {0x00, 0xC0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
        svc->WriteBurstReg(0x3E, paTable, 8);
        
        // Configure PATABLE reading depth for ASK/OOK (index 0 and 1)
        svc->_WriteRegPub(CC1101_FREND0, 0x11);

        // 5. Enter TX State
        svc->_CommandStrobe(CC1101_STX);
        vTaskDelay(pdMS_TO_TICKS(10)); // Allow PLL to lock
        
        // --- Transmit via RMT ---
        if (rmt_transmit(ch, enc, symbols.data(),
                         symbols.size() * sizeof(rmt_symbol_word_t), &tx_cfg) == ESP_OK) {
            rmt_tx_wait_all_done(ch, -1);
        }
    }
    
    // --- Cleanup ---
    // Return to IDLE state
    svc->_CommandStrobe(CC1101_SIDLE);
    
    // Restore original register values
    svc->_WriteRegPub(CC1101_PKTCTRL0, 0x30); // restore infinite packet mode
    svc->_WriteRegPub(0x02, 0x3F);            // restore IOCFG0 default
    
    rmt_disable(ch); rmt_del_encoder(enc); rmt_del_channel(ch);
    gpio_set_direction((gpio_num_t)gdo0_pin, GPIO_MODE_INPUT);
    return true;
}

// ---------------------------------------------------------------------------
// Transmit: RAW mode
// ---------------------------------------------------------------------------
bool Cc1101Service::TransmitRaw(const SubFileData& data) {
    if (data.raw_samples.empty()) { ESP_LOGE(TAG, "RAW buffer empty"); return false; }
    return rmt_transmit_raw(data.raw_samples, LORA_GDO0_PIN, this);
}

// ---------------------------------------------------------------------------
// Transmit: BinRAW mode (binary string → pulse timings via TE)
// ---------------------------------------------------------------------------
bool Cc1101Service::TransmitBinRaw(const SubFileData& data) {
    if (data.bin_data.empty() || data.te <= 0) {
        ESP_LOGE(TAG, "BinRAW: empty data or TE=0");
        return false;
    }
    std::vector<int32_t> buf;
    bool level = true;   // BinRAW starts HIGH
    for (char c : data.bin_data) {
        if (c == '1' || c == '0') {
            bool bit_val = (c == '1');
            int32_t pulse = level ? (int32_t)data.te : -(int32_t)data.te;
            buf.push_back(pulse);
            // In Flipper BinRAW each bit is one TE pulse; level alternates on bit change
            // Simple approach: level = bit_val
            level = bit_val;
        }
    }
    if (buf.empty()) { ESP_LOGE(TAG, "BinRAW: no pulses built"); return false; }
    return rmt_transmit_raw(buf, LORA_GDO0_PIN, this);
}

// ---------------------------------------------------------------------------
// Transmit: Key+Bit+TE mode (protocol-based, bit-bang via GPIO)
// ---------------------------------------------------------------------------
bool Cc1101Service::TransmitKey(const SubFileData& data) {
    if (data.bit <= 0 || data.te <= 0) {
        ESP_LOGE(TAG, "TransmitKey: bit=%d te=%d — invalid", data.bit, data.te);
        return false;
    }
    ESP_LOGI(TAG, "TransmitKey: key=%llu bit=%d te=%d",
             (unsigned long long)data.key, data.bit, data.te);

    // Set CC1101 to TX, async serial mode (pktFormat=3 for GDO0 output)
    WriteReg(CC1101_PKTCTRL0, 0x32);  // async serial, infinite packet
    CommandStrobe(CC1101_STX);
    vTaskDelay(pdMS_TO_TICKS(5));

    gpio_set_direction((gpio_num_t)LORA_GDO0_PIN, GPIO_MODE_OUTPUT);

    // Transmit MSB first, 3x repetition for reliability (similar to Bruce)
    for (int rep = 0; rep < 3; rep++) {
        for (int i = data.bit - 1; i >= 0; i--) {
            bool b = (data.key >> i) & 1;
            gpio_set_level(LORA_GDO0_PIN, b ? 1 : 0);
            esp_rom_delay_us(data.te);
        }
        gpio_set_level(LORA_GDO0_PIN, 0);
        esp_rom_delay_us(data.te * 4);   // inter-frame gap
    }

    CommandStrobe(CC1101_SIDLE);
    WriteReg(CC1101_PKTCTRL0, 0x30);  // restore infinite packet mode
    gpio_set_direction((gpio_num_t)LORA_GDO0_PIN, GPIO_MODE_INPUT);
    ESP_LOGI(TAG, "TransmitKey done.");
    return true;
}

// ---------------------------------------------------------------------------
// Hardcoded Transmissions
// ---------------------------------------------------------------------------
bool Cc1101Service::TransmitTeslaPortSignal() {
    if (is_replaying_ || is_jamming_) {
        ESP_LOGE(TAG, "Cannot transmit Tesla port signal, radio is busy.");
        return false;
    }

    ESP_LOGI(TAG, "Transmitting hardcoded Tesla Port Opener signal (433.92 MHz, AM650)");

    // Ensure a completely clean slate by completely tearing down and rebuilding SPI communication
    Deinit();
    if (!Init()) {
        ESP_LOGE(TAG, "Failed to re-initialize CC1101 for Tesla Port Opener");
        return false;
    }

    // Define signal config
    current_frequency_mhz_ = 433.92f;
    SetFrequency(current_frequency_mhz_);
    ApplyFlipperPreset("FuriHalSubGhzPresetOok650Async");

    is_replaying_ = true;

    // Transmit sequences repeatedly just like a real remote
    // We send it 5 times to ensure it is caught by the car
    bool success = true;
    for (int i = 0; i < 5; i++) {
        ESP_LOGI(TAG, "Sending Tesla sequence %d/5...", i + 1);
        if (!rmt_transmit_raw(TESLA_AM650_RAW, LORA_GDO0_PIN, this)) {
            success = false;
            break;
        }
        vTaskDelay(pdMS_TO_TICKS(50));
    }

    is_replaying_ = false;
    
    if (success) {
        ESP_LOGI(TAG, "Tesla Port Opener signal transmitted successfully.");
    } else {
        ESP_LOGE(TAG, "Failed to transmit Tesla Port Opener signal.");
    }
    
    return success;
}

// ---------------------------------------------------------------------------
// Public: ReplaySubFile — orchestrates parsing + transmission
// ---------------------------------------------------------------------------
void Cc1101Service::_SetReplaying(bool state) { is_replaying_ = state; }

bool Cc1101Service::ReplaySubFile(const std::string& filename) {
    if (is_replaying_ || is_jamming_) return false;
    if (!initialized_ && !Init()) return false;

    // Resolve path (handle SD card mount point, .sub extension, case-insensitive)
    std::string directory = SDCARD_MOUNT_POINT;
    std::string name_only = filename;
    size_t last_slash = filename.find_last_of('/');
    if (last_slash != std::string::npos) {
        directory = filename.substr(0, last_slash);
        name_only = filename.substr(last_slash + 1);
    }
    
    std::string resolved_path = FindFileCaseInsensitive(directory, name_only);
    
    // Fallback: if not found in root, check /sdcard/sub/
    if (resolved_path.empty() && directory == SDCARD_MOUNT_POINT) {
        resolved_path = FindFileCaseInsensitive(SDCARD_MOUNT_POINT "/sub", name_only);
    }
    if (resolved_path.empty() && directory == SDCARD_MOUNT_POINT) {
        resolved_path = FindFileCaseInsensitive(SDCARD_MOUNT_POINT "/subghz", name_only);
    }

    if (resolved_path.empty()) {
        ESP_LOGE(TAG, "File not found: %s", filename.c_str());
        return false;
    }

    // Parse the .sub file
    SubFileData parsed;
    if (!ParseSubFile(resolved_path, parsed)) return false;

    current_sub_path_ = resolved_path;
    sub_data_ = parsed;
    replay_buffer_ = parsed.raw_samples;   // keep for legacy access

    // Apply frequency
    float freq_mhz = parsed.frequency / 1000000.0f;
    if (freq_mhz < 1.0f) freq_mhz = parsed.frequency;  // already in MHz
    current_frequency_mhz_ = freq_mhz;

    // Apply preset / modulation
    if (!parsed.preset.empty()) {
        ApplyFlipperPreset(parsed.preset);
    } else {
        std::string lower = parsed.protocol;
        std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
        bool is_fsk = (lower.find("fsk") != std::string::npos ||
                       lower.find("msk") != std::string::npos ||
                       lower.find("gfsk") != std::string::npos);
        SetModulation(is_fsk ? "FSK" : "ASK");
    }
    SetFrequency(current_frequency_mhz_);

    is_replaying_ = true;

    // Dispatch transmission in a background task
    xTaskCreatePinnedToCore(
        [](void* arg) {
            Cc1101Service* svc = static_cast<Cc1101Service*>(arg);
            const SubFileData& d = svc->_GetSubData();
            bool ok = false;

            if (d.protocol == "BinRAW") {
                ESP_LOGI("Cc1101", "Transmit mode: BinRAW");
                ok = svc->TransmitBinRaw(d);
            } else if (d.protocol == "RAW" || d.protocol == "") {
                ESP_LOGI("Cc1101", "Transmit mode: RAW (%d samples)", (int)d.raw_samples.size());
                ok = svc->TransmitRaw(d);
            } else if (d.bit > 0 && d.te > 0) {
                // Named protocol (Princeton, CAME, Holtek...) or generic key bit-bang
                ESP_LOGI("Cc1101", "Transmit mode: Protocol '%s' key=%llu bit=%d te=%d",
                         d.protocol.c_str(), (unsigned long long)d.key, d.bit, d.te);
                ok = svc->TransmitProtocol(d);   // uses table; falls back to TransmitKey
            } else if (!d.raw_samples.empty()) {
                ESP_LOGW("Cc1101", "Transmit mode: RAW fallback");
                ok = svc->TransmitRaw(d);
            } else {
                ESP_LOGE("Cc1101", "No transmittable data found in .sub file.");
            }

            ESP_LOGI("Cc1101", "Replay %s.", ok ? "finished" : "failed");
            svc->_SetReplaying(false);
            vTaskDelete(NULL);
        },
        "replay_task", 6144, this, 5, (TaskHandle_t*)&replay_task_handle_, 1);

    return true;
}

bool Cc1101Service::StopReplay() {
    if (!is_replaying_) return false;
    is_replaying_ = false;
    if (initialized_) CommandStrobe(CC1101_SIDLE);
    ESP_LOGI(TAG, "Replay stopped.");
    return true;
}

// ---------------------------------------------------------------------------
// Faz 4: Generate unique .sub filename on SD card (raw_0.sub, raw_1.sub, ...)
// ---------------------------------------------------------------------------
std::string Cc1101Service::GenerateSubFilename() const {
    char buf[64];
    int idx = 0;
    // Try /sdcard/sub/ first, fall back to /sdcard/
    const char* dirs[] = {SDCARD_MOUNT_POINT "/sub", SDCARD_MOUNT_POINT};
    for (const char* dir : dirs) {
        DIR* d = opendir(dir);
        if (!d) continue;
        closedir(d);
        do {
            snprintf(buf, sizeof(buf), "%s/raw_%d.sub", dir, idx++);
        } while (access(buf, F_OK) == 0);
        return std::string(buf);
    }
    snprintf(buf, sizeof(buf), SDCARD_MOUNT_POINT "/raw_%d.sub", idx);
    return std::string(buf);
}

// ---------------------------------------------------------------------------
// Faz 4: Save RAW samples to Flipper-compatible .sub file
// ---------------------------------------------------------------------------
bool Cc1101Service::SaveRawToSubFile(const std::vector<int32_t>& samples,
                                     float freq_mhz,
                                     const std::string& filepath) {
    if (samples.empty()) { ESP_LOGE(TAG, "SaveRawToSubFile: empty buffer"); return false; }

    std::string path = filepath.empty() ? GenerateSubFilename() : filepath;
    FILE* f = fopen(path.c_str(), "w");
    if (!f) { ESP_LOGE(TAG, "Cannot create: %s", path.c_str()); return false; }

    // Header
    fprintf(f, "Filetype: Zhilly SubGhz File\n");
    fprintf(f, "Version 1\n");
    fprintf(f, "Frequency: %d\n", (int)(freq_mhz * 1000000.0f));
    fprintf(f, "Preset: FuriHalSubGhzPresetOok650Async\n");
    fprintf(f, "Protocol: RAW\n");
    fprintf(f, "RAW_Data:");

    // Body — per Flipper spec: max 512 values per RAW_Data line
    int count = 0;
    for (int32_t val : samples) {
        if (count > 0 && count % 512 == 0) fprintf(f, "\nRAW_Data:");
        fprintf(f, " %ld", (long)val);
        count++;
    }
    fprintf(f, "\n");
    fclose(f);
    ESP_LOGI(TAG, "Saved %d samples → %s", count, path.c_str());
    return true;
}

// ---------------------------------------------------------------------------
// Faz 5: Named protocol table — pulse definitions for common OOK protocols
//
// All timing is in TE (Transmission Element) multiples.
// Negative = LOW, positive = HIGH (as stored in .sub RAW_Data convention).
//
// Sources: Bruce firmware protocols/ + Flipper file format docs
// ---------------------------------------------------------------------------
static const ProtocolTableEntry kProtocolTable[] = {
    // name              pilot_H  pilot_L  z_H  z_L  1_H  1_L  stop_H  stop_L  msb
    {"Princeton",              1,      31,   1,   3,   3,   1,    0,      0,   true},
    {"CAME",                   1,      36,   1,   2,   2,   1,    0,      0,   true},
    {"CAME_TWEE",              1,      12,   1,   3,   3,   1,    0,      0,   true},
    {"NiceFlo12bit",           1,      36,   1,   2,   2,   1,    0,      0,   false},
    {"NiceFlo24bit",           1,      36,   1,   2,   2,   1,    0,      0,   false},
    {"Holtek",                 1,      36,   1,   2,   2,   1,    0,      0,   true},
    {"HoltekHT12E",            1,      36,   1,   2,   2,   1,    0,      0,   true},
    {"Ansonic",                2,      29,   1,   2,   2,   1,    0,      0,   true},
    {"Chamberlain",            0,       0,   1,   2,   2,   1,    1,     40,   true},
    {"Linear",                 1,      10,   1,   1,   2,   1,    0,      0,   true},
    {"Liftmaster",             1,      27,   2,   1,   1,   2,    0,      0,   true},
    {"GateTX",                 1,      36,   1,   2,   2,   1,    0,      0,   true},
    {"CAME_ATOMO",             1,      36,   1,   2,   2,   1,    0,      0,   true},
};
static const size_t kProtocolTableSize = sizeof(kProtocolTable) / sizeof(kProtocolTable[0]);

// ---------------------------------------------------------------------------
// Faz 5: Transmit using named protocol table
// ---------------------------------------------------------------------------
bool Cc1101Service::TransmitProtocol(const SubFileData& data) {
    if (data.te <= 0) { ESP_LOGE(TAG, "TransmitProtocol: TE=0"); return false; }
    if (data.bit <= 0) { ESP_LOGE(TAG, "TransmitProtocol: bit=0"); return false; }

    // Find the protocol entry (case-insensitive prefix match)
    const ProtocolTableEntry* entry = nullptr;
    std::string proto_lower = data.protocol;
    std::transform(proto_lower.begin(), proto_lower.end(), proto_lower.begin(), ::tolower);

    for (size_t i = 0; i < kProtocolTableSize; i++) {
        std::string tbl_lower = kProtocolTable[i].name;
        std::transform(tbl_lower.begin(), tbl_lower.end(), tbl_lower.begin(), ::tolower);
        if (proto_lower == tbl_lower || proto_lower.find(tbl_lower) == 0) {
            entry = &kProtocolTable[i];
            break;
        }
    }

    if (!entry) {
        // Fall back to generic Key+Bit+TE bit-bang
        ESP_LOGW(TAG, "Protocol '%s' not in table, falling back to TransmitKey", data.protocol.c_str());
        return TransmitKey(data);
    }

    ESP_LOGI(TAG, "TransmitProtocol: '%s' key=%llu bit=%d te=%d",
             entry->name, (unsigned long long)data.key, data.bit, data.te);

    // Build the RAW timing buffer from the protocol table
    std::vector<int32_t> buf;
    buf.reserve(200);

    // Pilot burst
    if (entry->pilot_high_te > 0) buf.push_back( entry->pilot_high_te * data.te);
    if (entry->pilot_low_te  > 0) buf.push_back(-entry->pilot_low_te  * data.te);

    // Data bits
    for (int i = entry->msb_first ? (data.bit - 1) : 0;
         entry->msb_first ? (i >= 0) : (i < data.bit);
         entry->msb_first ? i-- : i++) {
        bool bit = (data.key >> i) & 1;
        int h_te = bit ? entry->one_high_te : entry->zero_high_te;
        int l_te = bit ? entry->one_low_te  : entry->zero_low_te;
        buf.push_back( h_te * data.te);
        buf.push_back(-l_te * data.te);
    }

    // Stop bit
    if (entry->stop_high_te > 0) buf.push_back( entry->stop_high_te * data.te);
    if (entry->stop_low_te  > 0) buf.push_back(-entry->stop_low_te  * data.te);

    return rmt_transmit_raw(buf, LORA_GDO0_PIN, this);
}

// ---------------------------------------------------------------------------
// Jammer
// ---------------------------------------------------------------------------
static void JammerTask(void* arg) {
    Cc1101Service* svc = static_cast<Cc1101Service*>(arg);
    ESP_LOGI("Cc1101", "Jammer task started.");
    if (!svc->GetStatus() && !svc->Init()) {
        ESP_LOGE("Cc1101", "CC1101 init error for Jammer!");
        svc->StopJammer(); vTaskDelete(NULL); return;
    }

    // 1. Completely re-initialize radio to ensure a pristine state for the Jammer
    svc->Deinit();
    if (!svc->Init()) {
        ESP_LOGE("Cc1101", "CC1101 re-init error for Jammer!");
        svc->StopJammer(); vTaskDelete(NULL); return;
    }
    vTaskDelay(pdMS_TO_TICKS(5));

    // 2. Re-apply frequency + configure SW0/SW1 antenna switch
    //    This is essential: without it, CC1101 FREQ registers may not be set
    //    and the RF multiplexer (SW1=IO47, SW0=IO48) stays unconfigured
    float jam_freq = svc->_GetFrequency();
    if (jam_freq < 300.0f) jam_freq = 433.92f; // fallback
    svc->SetFrequency(jam_freq);
    ESP_LOGI("Cc1101", "Jammer freq: %.2f MHz", jam_freq);

    // 3. Apply Flipper preset for ASK/OOK (Sets MDMCFG2, DEVIATN, FREQ offset, etc.)
    svc->ApplyFlipperPreset("FuriHalSubGhzPresetOok650Async");

    // 4. Async serial TX mode (PKT_FORMAT=11, no FIFO)
    svc->_WriteRegPub(CC1101_PKTCTRL0, 0x32);

    // 5. Disable CC1101's own GDO0 output driver (IOCFG0 = 0x2E = Hi-Z)
    //    Prevents bus contention when ESP32 drives GPIO3 as OUTPUT
    svc->_WriteRegPub(0x02, 0x2E); // IOCFG0 = Hi-Z

    // 6. Set correct TX power in PATABLE for ASK/OOK (Burst write 0x3E)
    //    For ASK/OOK, CC1101 requires two power levels: index 0 (off), index 1 (on)
    //    0x00 = minimum power, 0xC0 = max power (~10dBm at 433MHz)
    uint8_t paTable[8] = {0x00, 0xC0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
    svc->WriteBurstReg(0x3E, paTable, 8);
    
    // Set FREND0.PA_POWER = 1 to tell CC1101 to use index 0 and 1 for ASK/OOK
    svc->_WriteRegPub(CC1101_FREND0, 0x11);

    // 7. Configure GDO0 (GPIO3) as OUTPUT *before* STX (Bruce's order)
    gpio_set_direction((gpio_num_t)LORA_GDO0_PIN, GPIO_MODE_OUTPUT);
    gpio_set_level(LORA_GDO0_PIN, 0);

    // 8. Start TX mode (equivalent to ELECHOUSE_cc1101.SetTx())
    svc->_CommandStrobe(CC1101_STX);
    vTaskDelay(pdMS_TO_TICKS(10)); // allow PLL to lock

    ESP_LOGI("Cc1101", "Jammer TX active, jamming...");

    // ----- Jammer loop: 10ms work window + 1ms FreeRTOS yield -----
    // This mirrors Bruce's run_full_jammer:
    //   - Drive GDO0 HIGH (continuous carrier)
    //   - Brief LOW glitches every ~100us to create wideband noise
    while (svc->IsJamming()) {
        int64_t window_end = esp_timer_get_time() + 10000; // 10ms window
        gpio_set_level(LORA_GDO0_PIN, 1);

        while (esp_timer_get_time() < window_end && svc->IsJamming()) {
            gpio_set_level(LORA_GDO0_PIN, 0);
            esp_rom_delay_us(1);
            gpio_set_level(LORA_GDO0_PIN, 1);
            esp_rom_delay_us(99); // ~100us glitch cycle
        }

        // Brief LOW + yield to feed FreeRTOS IDLE task (watchdog)
        gpio_set_level(LORA_GDO0_PIN, 0);
        vTaskDelay(pdMS_TO_TICKS(1));
    }

    // --- Cleanup ---
    // Return GDO0 to Hi-Z and radio to IDLE.
    // We already checked svc->IsJamming() is false here.
    gpio_set_level(LORA_GDO0_PIN, 0);
    svc->_CommandStrobe(CC1101_SIDLE);
    svc->_WriteRegPub(CC1101_PKTCTRL0, 0x30); // restore infinite packet mode
    svc->_WriteRegPub(0x02, 0x3F);            // restore IOCFG0 default
    gpio_set_direction((gpio_num_t)LORA_GDO0_PIN, GPIO_MODE_INPUT);
    
    ESP_LOGI("Cc1101", "Jammer task finished.");
    svc->_SetJammingTaskHandle(nullptr); // Clear handle before exit
    vTaskDelete(NULL);
}

static void JammerTimeoutCb(void* arg) {
    Cc1101Service* svc = static_cast<Cc1101Service*>(arg);
    if (svc->IsJamming()) svc->StopJammer();
}

bool Cc1101Service::StartJammer(uint32_t duration_ms) {
    if (is_jamming_) return true;
    if (is_replaying_) return false;
    is_jamming_ = true;
    
    xTaskCreatePinnedToCore(JammerTask, "jammer_task", 6144, this, 5,
                            &jammer_task_handle_, 1);
    if (duration_ms > 0) {
        esp_timer_create_args_t args = {};
        args.callback = &JammerTimeoutCb;
        args.arg = this;
        args.name = "jammer_to";
        esp_timer_create(&args, &jammer_timer_);
        esp_timer_start_once(jammer_timer_, duration_ms * 1000ULL);
        ESP_LOGI(TAG, "Jammer timeout set: %lu ms", duration_ms);
    }
    ESP_LOGI(TAG, "Jammer starting (%lu ms)", duration_ms);
    return true;
}

bool Cc1101Service::StopJammer() {
    if (!is_jamming_) return false;
    is_jamming_ = false;
    // Stop and delete the timeout timer if it's still running
    if (jammer_timer_) {
        esp_timer_stop(jammer_timer_);
        esp_timer_delete(jammer_timer_);
        jammer_timer_ = nullptr;
    }
    // We don't call CommandStrobe(SIDLE) here anymore.
    // The JammerTask itself will detect is_jamming_ is false and cleanup correctly
    // avoiding SPI bus contention during the stop process.
    ESP_LOGI(TAG, "Jammer stop requested");
    return true;
}
