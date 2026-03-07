#include "cc1101_service.h"
#include <cJSON.h>
#include <dirent.h>
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

static const Cc1101RegisterSetting bruce_ook_433[] = {
    {0x0B, 0x06}, {0x0C, 0x00}, {0x10, 0xC8}, {0x11, 0x93}, {0x12, 0x30}, {0x13, 0x22},
    {0x14, 0xF8}, {0x15, 0x34}, {0x17, 0x30}, {0x18, 0x18}, {0x19, 0x16}, {0x1A, 0x6C},
    {0x1B, 0x43}, {0x1C, 0x40}, {0x1D, 0x91}, {0x21, 0x56}, {0x22, 0x10}, {0x23, 0xE9},
    {0x24, 0x2A}, {0x25, 0x00}, {0x26, 0x1F}, {0x08, 0x32}};

#define CC1101_SRES 0x30
#define CC1101_SRX 0x34
#define CC1101_STX 0x35
#define CC1101_SIDLE 0x36
#define CC1101_SFRX 0x3A
#define CC1101_SFTX 0x3B
#define CC1101_VERSION 0x31
#define CC1101_DEVIATN 0x15
#define CC1101_PKTCTRL0 0x08
#define CC1101_RSSI 0x34

Cc1101Service::Cc1101Service() {}

Cc1101Service::~Cc1101Service() {
    if (rmt_encoder_) {
        rmt_del_encoder(rmt_encoder_);
    }
    if (rmt_tx_channel_) {
        rmt_del_channel(rmt_tx_channel_);
    }
    if (spi_handle_) {
        spi_bus_remove_device(spi_handle_);
    }
}

void Cc1101Service::CommandStrobe(uint8_t cmd) {
    if (!spi_handle_)
        return;
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
    if (!spi_handle_)
        return;
    gpio_set_level(LORA_CS_PIN, 0);
    spi_transaction_t t = {};
    t.length = 16;
    t.flags = SPI_TRANS_USE_TXDATA;
    t.tx_data[0] = addr;
    t.tx_data[1] = data;
    spi_device_transmit(spi_handle_, &t);
    gpio_set_level(LORA_CS_PIN, 1);
}

uint8_t Cc1101Service::ReadReg(uint8_t addr) {
    if (!spi_handle_)
        return 0;
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
    gpio_set_level(LORA_CS_PIN, 1);
    vTaskDelay(pdMS_TO_TICKS(1));
    gpio_set_level(LORA_CS_PIN, 0);
    vTaskDelay(pdMS_TO_TICKS(1));
    gpio_set_level(LORA_CS_PIN, 1);
    vTaskDelay(pdMS_TO_TICKS(41));

    gpio_set_level(LORA_CS_PIN, 0);
    vTaskDelay(pdMS_TO_TICKS(10));
    CommandStrobe(CC1101_SRES);
    vTaskDelay(pdMS_TO_TICKS(10));
    gpio_set_level(LORA_CS_PIN, 1);
}

bool Cc1101Service::Deinit() {
    if (!initialized_)
        return true;

    if (spi_handle_) {
        esp_err_t ret = spi_bus_remove_device(spi_handle_);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to remove SPI device: %s", esp_err_to_name(ret));
            return false;
        }
        spi_handle_ = nullptr;
    }

    initialized_ = false;
    ESP_LOGI(TAG, "CC1101 SPI device removed successfully.");
    return true;
}

bool Cc1101Service::Init() {
    if (initialized_)
        return true;

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

    SetRfSettings();

    uint8_t version = ReadReg(CC1101_VERSION | 0xC0);
    ESP_LOGI(TAG, "CC1101 version: 0x%02X", version);
    if (version == 0x00 || version == 0xFF) {
        ESP_LOGE(TAG, "CC1101 SPI error! Version: 0x%02X", version);
    }

    initialized_ = true;
    ESP_LOGI(TAG, "CC1101 ready. Call load_presets when SD card is ready.");
    return true;
}

void Cc1101Service::SetRfSettings() { SetFrequency(433.92f); }

bool Cc1101Service::SetFrequency(float mhz) {
    if (!initialized_)
        return false;

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
    if (!initialized_)
        return false;

    if (presets_.count(type)) {
        return ApplyPreset(type);
    }

    if (type == "ASK" || type == "OOK" || type == "AM") {
        for (size_t i = 0; i < sizeof(bruce_ook_433) / sizeof(bruce_ook_433[0]); i++) {
            WriteReg(bruce_ook_433[i].reg, bruce_ook_433[i].val);
        }
        current_modulation_ = "AM (ASK)";
        ESP_LOGI(TAG, "ASK/OOK modu uygulandi (Bruce Golden)");
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

float Cc1101Service::ReadRssi() {
    if (!initialized_)
        return -100.0f;

    uint8_t state = ReadReg(0x35 | 0xC0) & 0x1F;
    if (state != 0x0D) {
        CommandStrobe(CC1101_SRX);
        vTaskDelay(pdMS_TO_TICKS(2));
    }

    uint8_t rssi_raw = ReadReg(0x34 | 0xC0);
    float rssi_dbm;
    if (rssi_raw >= 128) {
        rssi_dbm = ((float)rssi_raw - 256) / 2.0f - 74.0f;
    } else {
        rssi_dbm = (float)rssi_raw / 2.0f - 74.0f;
    }
    return rssi_dbm;
}

bool Cc1101Service::GetStatus() { return initialized_; }

uint8_t Cc1101Service::GetChipVersion() { return ReadReg(CC1101_VERSION | 0xC0); }

void Cc1101Service::DumpRegisters() {
    ESP_LOGI(TAG, "--- CC1101 REGISTER DUMP ---");
    for (uint8_t i = 0; i <= 0x2E; i++) {
        ESP_LOGI(TAG, "Reg 0x%02X: 0x%02X", i, ReadReg(i | 0xC0));
    }
    ESP_LOGI(TAG, "----------------------------");
}

bool Cc1101Service::LoadPresets(const std::string& path) {
    FILE* f = fopen(path.c_str(), "r");
    if (!f) {
        ESP_LOGE(TAG, "Failed to open preset file: %s", path.c_str());
        return false;
    }

    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);

    char* buffer = (char*)malloc(size + 1);
    if (!buffer) {
        fclose(f);
        return false;
    }
    fread(buffer, 1, size, f);
    buffer[size] = '\0';
    fclose(f);

    cJSON* root = cJSON_Parse(buffer);
    free(buffer);
    if (!root) {
        ESP_LOGE(TAG, "JSON parse error: %s", path.c_str());
        return false;
    }

    cJSON* arr = cJSON_GetObjectItem(root, "presets");
    if (cJSON_IsArray(arr)) {
        presets_.clear();
        int n = cJSON_GetArraySize(arr);
        for (int i = 0; i < n; i++) {
            cJSON* item = cJSON_GetArrayItem(arr, i);
            Cc1101Preset p;
            cJSON* name = cJSON_GetObjectItem(item, "name");
            cJSON* mod = cJSON_GetObjectItem(item, "modulation");
            cJSON* freq = cJSON_GetObjectItem(item, "frequency");
            if (name)
                p.name = name->valuestring;
            if (mod)
                p.modulation = mod->valuestring;
            if (freq)
                p.frequency = (float)freq->valuedouble;

            cJSON* regs = cJSON_GetObjectItem(item, "registers");
            if (cJSON_IsArray(regs)) {
                for (int j = 0; j < cJSON_GetArraySize(regs); j++) {
                    cJSON* ritem = cJSON_GetArrayItem(regs, j);
                    Cc1101RegisterSetting rs;
                    cJSON* r = cJSON_GetObjectItem(ritem, "reg");
                    cJSON* v = cJSON_GetObjectItem(ritem, "val");
                    if (r && v) {
                        rs.reg = (uint8_t)strtol(r->valuestring, nullptr, 0);
                        rs.val = (uint8_t)strtol(v->valuestring, nullptr, 0);
                        p.registers.push_back(rs);
                    }
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
    if (it == presets_.end()) {
        ESP_LOGE(TAG, "Preset bulunamadi: %s", preset_name.c_str());
        return false;
    }
    const auto& p = it->second;
    for (const auto& rs : p.registers) {
        WriteReg(rs.reg, rs.val);
    }
    current_modulation_ = p.modulation;
    ESP_LOGI(TAG, "Preset uygulandi: %s", preset_name.c_str());
    return true;
}

std::string Cc1101Service::ListSdFiles() const {
    DIR* dir = opendir(SDCARD_MOUNT_POINT);
    if (!dir) {
        return "Error: failed to open /sdcard. Is SD card inserted?";
    }

    std::string result = "SD Card files (/sdcard):\n";
    struct dirent* entry;
    int count = 0;
    while ((entry = readdir(dir)) != NULL) {
        std::string name = entry->d_name;
        if (name == "." || name == "..")
            continue;

        std::string lower = name;
        std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);

        std::string label = "[file]";
        if (entry->d_type == DT_DIR)
            label = "[klasor]";
        else if (lower.rfind(".sub") != lower.npos)
            label = "[sub]";
        else if (lower.rfind(".json") != lower.npos)
            label = "[json]";
        else if (lower.rfind(".csv") != lower.npos)
            label = "[csv]";
        else if (lower.rfind(".txt") != lower.npos)
            label = "[txt]";

        result += label + " " + name + "\n";
        count++;
    }
    closedir(dir);

    if (count == 0)
        return "No files found on SD card.";
    result += "Total: " + std::to_string(count) + " files.";
    return result;
}

void Cc1101Service::_SetReplaying(bool state) { is_replaying_ = state; }

bool Cc1101Service::ReplaySubFile(const std::string& filename) {
    if (is_replaying_ || is_jamming_)
        return false;

    if (!initialized_) {
        if (!Init()) {
            return false;
        }
    }

    std::string directory = SDCARD_MOUNT_POINT;
    std::string name_only = filename;

    size_t last_slash = filename.find_last_of('/');
    if (last_slash != std::string::npos) {
        directory = filename.substr(0, last_slash);
        name_only = filename.substr(last_slash + 1);
    }

    std::string resolved_path = FindFileCaseInsensitive(directory, name_only);
    if (resolved_path.empty()) {
        ESP_LOGE(TAG, "File not found (case-insensitive): %s/%s", directory.c_str(),
                 name_only.c_str());
        return false;
    }

    FILE* f = fopen(resolved_path.c_str(), "r");
    if (!f) {
        return false;
    }

    replay_buffer_.clear();
    replay_buffer_.reserve(8192);
    char line[1024];
    bool use_fsk = false;
    bool in_raw = false;
    int read_count = 0;

    while (fgets(line, sizeof(line), f)) {
        size_t len = strlen(line);
        while (len > 0 && (line[len - 1] == '\r' || line[len - 1] == '\n'))
            line[--len] = '\0';

        if (strstr(line, "Frequency:")) {
            uint32_t freq_hz = 0;
            if (sscanf(line, "Frequency: %lu", &freq_hz) == 1) {
                current_frequency_mhz_ = (float)freq_hz / 1000000.0f;
            }
        } else if (strstr(line, "Preset:")) {
            std::string l(line);
            std::transform(l.begin(), l.end(), l.begin(), ::tolower);
            use_fsk = (l.find("fsk") != std::string::npos);
        } else if (strstr(line, "RAW_Data:")) {
            in_raw = true;
            char* data_ptr = strstr(line, "RAW_Data:") + 9;
            char* token = strtok(data_ptr, " \t");
            while (token != NULL) {
                int32_t val = atol(token);
                if (val != 0)
                    replay_buffer_.push_back(val);
                token = strtok(NULL, " \t");
            }
        } else if (in_raw && strchr(line, ':') == NULL && len > 0) {
            char* token = strtok(line, " \t");
            while (token != NULL) {
                int32_t val = atol(token);
                if (val != 0)
                    replay_buffer_.push_back(val);
                token = strtok(NULL, " \t");
            }
        } else if (in_raw && strchr(line, ':') != NULL) {
            in_raw = false;
        }

        if (++read_count % 20 == 0) {
            vTaskDelay(0);
        }
    }

    fclose(f);
    ESP_LOGI(TAG, "SD reading complete. Data size: %d", replay_buffer_.size());

    if (replay_buffer_.empty()) {
        ESP_LOGE(TAG, "RAW_Data is empty!");
        return false;
    }

    current_sub_path_ = resolved_path;
    current_modulation_ = use_fsk ? "FM (FSK)" : "AM (ASK)";

    SetModulation(current_modulation_);
    SetFrequency(current_frequency_mhz_);

    is_replaying_ = true;

    xTaskCreatePinnedToCore(
        [](void* arg) {
            Cc1101Service* svc = static_cast<Cc1101Service*>(arg);
            const std::vector<int32_t>& buf = svc->_GetRawBuffer();
            ESP_LOGI("Cc1101", "Replay started. Samples: %d", buf.size());

            rmt_tx_channel_config_t tx_chan_config = {.gpio_num = (gpio_num_t)LORA_GDO0_PIN,
                                                      .clk_src = RMT_CLK_SRC_DEFAULT,
                                                      .resolution_hz = 1000000,  
                                                      .mem_block_symbols = 64,
                                                      .trans_queue_depth = 4,
                                                      .flags = {.invert_out = false,
                                                                .with_dma = false,
                                                                .io_loop_back = false,
                                                                .io_od_mode = false}};

            rmt_channel_handle_t rmt_tx_channel = nullptr;
            if (rmt_new_tx_channel(&tx_chan_config, &rmt_tx_channel) != ESP_OK) {
                ESP_LOGE("Cc1101", "Failed to create RMT TX channel.");
                svc->_SetReplaying(false);
                vTaskDelete(NULL);
                return;
            }

            rmt_copy_encoder_config_t copy_encoder_config = {};
            rmt_encoder_handle_t rmt_encoder = nullptr;
            if (rmt_new_copy_encoder(&copy_encoder_config, &rmt_encoder) != ESP_OK) {
                ESP_LOGE("Cc1101", "Failed to create RMT Encoder.");
                rmt_del_channel(rmt_tx_channel);
                svc->_SetReplaying(false);
                vTaskDelete(NULL);
                return;
            }

            if (rmt_enable(rmt_tx_channel) != ESP_OK) {
                ESP_LOGE("Cc1101", "RMT Enable error.");
                rmt_del_encoder(rmt_encoder);
                rmt_del_channel(rmt_tx_channel);
                svc->_SetReplaying(false);
                vTaskDelete(NULL);
                return;
            }

            svc->_CommandStrobe(CC1101_STX);
            vTaskDelay(pdMS_TO_TICKS(5));

            std::vector<rmt_symbol_word_t> rmt_symbols;
            rmt_symbols.reserve(buf.size() / 2 + 1);

            rmt_symbol_word_t current_symbol;
            bool high_part_filled = false;

            for (size_t i = 0; i < buf.size(); i++) {
                int32_t val = buf[i];
                uint32_t duration = (val > 0) ? (uint32_t)val : (uint32_t)(-val);

                if (duration > 32767)
                    duration = 32767;

                if (val > 0) {
                    current_symbol.duration1 = duration;
                    current_symbol.level1 = 1;
                    high_part_filled = true;
                } else {
                    current_symbol.duration0 = duration;
                    current_symbol.level0 = 0;

                    if (high_part_filled) {
                        rmt_symbols.push_back(current_symbol);
                        high_part_filled = false;
                    } else {
                        ESP_LOGW("Cc1101", "Unexpected Low without High in RAW sequence");

                        current_symbol.duration1 = 1;
                        current_symbol.level1 = 1;
                        rmt_symbols.push_back(current_symbol);
                    }
                }
            }

            if (high_part_filled) {
                current_symbol.duration0 = 1;
                current_symbol.level0 = 0;
                rmt_symbols.push_back(current_symbol);
            }

            rmt_transmit_config_t tx_config = {.loop_count = 0, .flags = {.eot_level = 0}};

            if (rmt_symbols.size() > 0) {
                if (rmt_transmit(rmt_tx_channel, rmt_encoder, rmt_symbols.data(),
                                 rmt_symbols.size() * sizeof(rmt_symbol_word_t),
                                 &tx_config) == ESP_OK) {
                    rmt_tx_wait_all_done(rmt_tx_channel, -1);
                } else {
                    ESP_LOGE("Cc1101", "RMT transmit failed.");
                }
            }

            svc->_CommandStrobe(CC1101_SIDLE);

            rmt_disable(rmt_tx_channel);
            rmt_del_encoder(rmt_encoder);
            rmt_del_channel(rmt_tx_channel);

            gpio_set_direction((gpio_num_t)LORA_GDO0_PIN, GPIO_MODE_INPUT);

            svc->_SetReplaying(false);
            ESP_LOGI("Cc1101", "Replay finished, CC1101 closed.");
            vTaskDelete(NULL);
        },
        "replay_rmt", 4096, this, 5, (TaskHandle_t*)&replay_task_handle_, 1);

    return true;
}

bool Cc1101Service::StopReplay() {
    if (!is_replaying_)
        return false;
    is_replaying_ = false;

    if (initialized_) {
        CommandStrobe(CC1101_SIDLE);
    }
    ESP_LOGI(TAG, "Replay stopped.");
    return true;
}

static void JammerTask(void* arg) {
    Cc1101Service* svc = static_cast<Cc1101Service*>(arg);
    ESP_LOGI("Cc1101", "Jammer task started.");

    if (!svc->GetStatus()) {
        if (!svc->Init()) {
            ESP_LOGE("Cc1101", "CC1101 init error for Jammer!");
            svc->StopJammer();
            vTaskDelete(NULL);
            return;
        }
    }
    gpio_set_direction(LORA_GDO0_PIN, GPIO_MODE_OUTPUT);
    gpio_set_level(LORA_GDO0_PIN, 0);
    svc->_CommandStrobe(CC1101_STX);

    while (svc->IsJamming()) {
        for (uint32_t pw = 10; pw <= 500 && svc->IsJamming(); pw += 10) {
            for (int d = 0; d < 3 && svc->IsJamming(); d++) {
                gpio_set_level(LORA_GDO0_PIN, 1);
                esp_rom_delay_us(pw);
                gpio_set_level(LORA_GDO0_PIN, 0);
                esp_rom_delay_us(pw + (pw % 23));
                vTaskDelay(pdMS_TO_TICKS(1));
            }
        }
        for (int i = 0; i < 100 && svc->IsJamming(); i++) {
            uint32_t pw = 5 + (esp_timer_get_time() % 50);
            gpio_set_level(LORA_GDO0_PIN, 1);
            esp_rom_delay_us(pw);
            gpio_set_level(LORA_GDO0_PIN, 0);
            esp_rom_delay_us(5 + (esp_timer_get_time() % 90));
        }
        vTaskDelay(pdMS_TO_TICKS(1));
    }

    gpio_set_level(LORA_GDO0_PIN, 0);
    svc->_CommandStrobe(CC1101_SIDLE);
    ESP_LOGI("Cc1101", "Jammer task finished, CC1101 closed.");
    vTaskDelete(NULL);
}

static void JammerTimeoutCb(void* arg) {
    Cc1101Service* svc = static_cast<Cc1101Service*>(arg);
    if (svc->IsJamming())
        svc->StopJammer();
}

bool Cc1101Service::StartJammer(uint32_t duration_ms) {
    if (is_jamming_)
        return true;
    if (is_replaying_)
        return false;

    is_jamming_ = true;

    xTaskCreatePinnedToCore(JammerTask, "jammer_task", 4096, this, 5,
                            (TaskHandle_t*)&jammer_task_handle_, 1);

    if (duration_ms > 0) {
        esp_timer_create_args_t args = {};
        args.callback = &JammerTimeoutCb;
        args.arg = this;
        args.name = "jammer_to";
        esp_timer_handle_t timer;
        esp_timer_create(&args, &timer);
        esp_timer_start_once(timer, duration_ms * 1000ULL);
    }

    ESP_LOGI(TAG, "Jammer starting (\%lu ms)", duration_ms);
    return true;
}

bool Cc1101Service::StopJammer() {
    if (!is_jamming_)
        return false;
    is_jamming_ = false;
    CommandStrobe(CC1101_SIDLE);
    gpio_set_level(LORA_GDO0_PIN, 0);
    ESP_LOGI(TAG, "Jammer stopped");
    return true;
}

std::string Cc1101Service::FindFileCaseInsensitive(const std::string& directory,
                                                   const std::string& filename) {
    DIR* dir = opendir(directory.c_str());
    if (!dir)
        return "";

    std::string search_name = filename;
    std::transform(search_name.begin(), search_name.end(), search_name.begin(), ::tolower);

    if (search_name.find('.') == std::string::npos) {
        search_name += ".sub";
    }

    struct dirent* de;
    std::string found_path = "";

    while ((de = readdir(dir)) != NULL) {
        if (de->d_type == DT_REG) {
            std::string entry_name = de->d_name;
            std::string entry_lower = entry_name;
            std::transform(entry_lower.begin(), entry_lower.end(), entry_lower.begin(), ::tolower);

            if (entry_lower == search_name) {
                found_path = directory + "/" + entry_name;
                break;
            }
        }
    }
    closedir(dir);
    return found_path;
}
