#include "ir_service.h"
#include <sys/stat.h>
#include <sys/types.h>
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <string>
#include <vector>
#include "boards/t-embed/config.h"
#include "driver/gpio.h"
#include "driver/rmt_rx.h"
#include "driver/rmt_tx.h"
#include "esp_log.h"
#include "esp_rom_sys.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char* TAG = "IrService";

#include "tvbgone_codes.h"

IrService::IrService() {}
IrService::~IrService() { Deinit(); }

bool IrService::Init() {
    if (initialized_)
        return true;

    rmt_tx_channel_config_t tx_cfg = {};
    tx_cfg.gpio_num = (gpio_num_t)IR_TX_GPIO;
    tx_cfg.clk_src = RMT_CLK_SRC_DEFAULT;
    tx_cfg.resolution_hz = RMT_CLK_RES_HZ;
    tx_cfg.mem_block_symbols = 64;
    tx_cfg.trans_queue_depth = 4;
    tx_cfg.flags.invert_out = false;
    tx_cfg.flags.with_dma = false;

    esp_err_t ret = rmt_new_tx_channel(&tx_cfg, &tx_channel_);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "TX kanalı oluşturulamadı: %s", esp_err_to_name(ret));
        return false;
    }

    rmt_carrier_config_t carrier_cfg = {};
    carrier_cfg.frequency_hz = 38000;
    carrier_cfg.duty_cycle = 0.33f;
    rmt_apply_carrier(tx_channel_, &carrier_cfg);

    rmt_copy_encoder_config_t enc_cfg = {};
    ret = rmt_new_copy_encoder(&enc_cfg, &tx_encoder_);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Encoder oluşturulamadı: %s", esp_err_to_name(ret));
        return false;
    }

    rmt_enable(tx_channel_);

    initialized_ = true;
    ESP_LOGI(TAG, "IR servisi başlatıldı (Sadece TX). TX=GPIO%d", IR_TX_GPIO);
    return true;
}

void IrService::Deinit() {
    if (!initialized_)
        return;
    StopJammer();
    StopTvBGone();
    if (tx_channel_) {
        rmt_disable(tx_channel_);
        rmt_del_channel(tx_channel_);
        tx_channel_ = nullptr;
    }
    if (tx_encoder_) {
        rmt_del_encoder(tx_encoder_);
        tx_encoder_ = nullptr;
    }
    initialized_ = false;
    ESP_LOGI(TAG, "IR servisi kapatıldı.");
}

bool IrService::SendRaw(const std::vector<uint16_t>& durations, uint32_t freq_hz) {
    if (!initialized_ || !tx_channel_ || !tx_encoder_)
        return false;
    if (durations.empty())
        return false;

    rmt_disable(tx_channel_);
    rmt_carrier_config_t carrier_cfg = {};
    carrier_cfg.frequency_hz = freq_hz;
    carrier_cfg.duty_cycle = 0.33f;
    rmt_apply_carrier(tx_channel_, &carrier_cfg);
    rmt_enable(tx_channel_);

    size_t n = durations.size();
    std::vector<rmt_symbol_word_t> symbols;
    symbols.reserve(n / 2 + 1);

    for (size_t i = 0; i + 1 < n; i += 2) {
        rmt_symbol_word_t sym = {};
        sym.duration0 = durations[i];
        sym.level0 = 1;
        sym.duration1 = durations[i + 1];
        sym.level1 = 0;
        symbols.push_back(sym);
    }
    if (n % 2 != 0) {
        rmt_symbol_word_t sym = {};
        sym.duration0 = durations[n - 1];
        sym.level0 = 1;
        sym.duration1 = 3000;
        sym.level1 = 0;
        symbols.push_back(sym);
    }

    rmt_transmit_config_t tx_config = {};
    tx_config.loop_count = 0;

    esp_err_t ret = rmt_transmit(tx_channel_, tx_encoder_, symbols.data(),
                                 symbols.size() * sizeof(rmt_symbol_word_t), &tx_config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "RMT TX error: %s", esp_err_to_name(ret));
        return false;
    }

    rmt_tx_wait_all_done(tx_channel_, pdMS_TO_TICKS(500));
    return true;
}

bool IrService::SendNEC(uint32_t address, uint32_t command) {
    std::vector<uint16_t> raw;
    raw.reserve(68);
    raw.push_back(9000);
    raw.push_back(4500);

    uint32_t data = (address & 0xFF) | ((~address & 0xFF) << 8) | ((command & 0xFF) << 16) |
                    ((~command & 0xFF) << 24);

    for (int i = 0; i < 32; i++) {
        raw.push_back(562);
        raw.push_back((data >> i) & 1 ? 1687 : 562);
    }
    raw.push_back(562);
    return SendRaw(raw, 38000);
}

bool IrService::SendRC5(uint32_t address, uint32_t command) {
    uint16_t data = ((1 & 1) << 13) | ((1 & 1) << 12) | ((address & 0x1F) << 6) | (command & 0x3F);
    std::vector<uint16_t> raw;
    const uint16_t half = 889;
    for (int i = 13; i >= 0; i--) {
        bool bit = (data >> i) & 1;
        if (bit) {
            raw.push_back(half);
            raw.push_back(half);
        } else {
            raw.push_back(half);
            raw.push_back(half);
        }
    }
    return SendRaw(raw, 36000);
}

bool IrService::SendRC6(uint32_t address, uint32_t command) {
    std::vector<uint16_t> raw;
    raw.push_back(2664);
    raw.push_back(888);

    uint32_t data = ((address & 0xFF) << 8) | (command & 0xFF);
    for (int i = 15; i >= 0; i--) {
        bool bit = (data >> i) & 1;
        raw.push_back(444);
        raw.push_back(bit ? 444 : 888);
    }
    raw.push_back(444);
    return SendRaw(raw, 36000);
}

bool IrService::SendSamsung(uint32_t address, uint32_t command) {
    std::vector<uint16_t> raw;
    raw.push_back(4500);
    raw.push_back(4500);
    uint32_t data = (address & 0xFF) | ((address & 0xFF) << 8) | ((command & 0xFF) << 16) |
                    ((command & 0xFF) << 24);
    for (int i = 0; i < 32; i++) {
        raw.push_back(560);
        raw.push_back((data >> i) & 1 ? 1690 : 560);
    }
    raw.push_back(560);
    return SendRaw(raw, 38000);
}

bool IrService::SendSony(uint32_t address, uint32_t command, uint8_t bits) {
    std::vector<uint16_t> raw;
    raw.push_back(2400);
    raw.push_back(600);
    uint32_t data = (command & 0x7F) | ((address & 0x1F) << 7);
    for (uint8_t i = 0; i < bits; i++) {
        raw.push_back((data >> i) & 1 ? 1200 : 600);
        raw.push_back(600);
    }
    return SendRaw(raw, 40000);
}

bool IrService::SendCode(const IrCode& code) {
    if (code.type == "raw") {
        return SendRaw(code.raw_data, code.frequency);
    }
    std::string p = code.protocol;
    std::transform(p.begin(), p.end(), p.begin(), ::tolower);
    if (p == "nec" || p == "nec42")
        return SendNEC(code.address, code.command);
    if (p == "necext" || p == "nec42ext")
        return SendNEC(code.address, code.command);
    if (p == "rc5" || p == "rc5x")
        return SendRC5(code.address, code.command);
    if (p == "rc6")
        return SendRC6(code.address, code.command);
    if (p == "samsung32")
        return SendSamsung(code.address, code.command);
    if (p == "sirc")
        return SendSony(code.address, code.command, 12);
    if (p == "sirc15")
        return SendSony(code.address, code.command, 15);
    if (p == "sirc20")
        return SendSony(code.address, code.command, 20);
    ESP_LOGW(TAG, "Bilinmeyen protokol: %s", code.protocol.c_str());
    return false;
}

std::vector<IrCode> IrService::ParseIrFile(const std::string& filepath) {
    std::vector<IrCode> codes;
    FILE* f = fopen(filepath.c_str(), "r");
    if (!f) {
        ESP_LOGE(TAG, "Failed to open file: %s", filepath.c_str());
        return codes;
    }

    char line[512];
    IrCode current;
    bool in_code = false;

    auto trim = [](std::string& s) {
        size_t start = s.find_first_not_of(" \t\r\n");
        size_t end = s.find_last_not_of(" \t\r\n");
        s = (start == std::string::npos) ? "" : s.substr(start, end - start + 1);
    };

    auto parse_hex = [](const std::string& s) -> uint32_t {
        if (s.empty())
            return 0;
        char* end;
        return (uint32_t)strtoul(s.c_str(), &end, 16);
    };

    while (fgets(line, sizeof(line), f)) {
        std::string l(line);
        trim(l);
        if (l.empty() || l[0] == '\0')
            continue;

        if (l[0] == '#') {
            if (in_code && !current.name.empty()) {
                codes.push_back(current);
                current = IrCode{};
                in_code = false;
            }
            continue;
        }

        size_t colon = l.find(':');
        if (colon == std::string::npos)
            continue;

        std::string key = l.substr(0, colon);
        std::string val = l.substr(colon + 1);
        trim(key);
        trim(val);
        std::transform(key.begin(), key.end(), key.begin(), ::tolower);

        if (key == "name") {
            current.name = val;
            in_code = true;
        } else if (key == "type") {
            current.type = val;
            std::transform(current.type.begin(), current.type.end(), current.type.begin(),
                           ::tolower);
        } else if (key == "protocol") {
            current.protocol = val;
        } else if (key == "frequency") {
            current.frequency = (uint32_t)atoi(val.c_str());
        } else if (key == "bits") {
            current.bits = (uint8_t)atoi(val.c_str());
        } else if (key == "address") {
            val.erase(std::remove(val.begin(), val.end(), ' '), val.end());
            current.address = parse_hex(val);
        } else if (key == "command") {
            val.erase(std::remove(val.begin(), val.end(), ' '), val.end());
            current.command = parse_hex(val);
        } else if (key == "data" || key == "value" || key == "state") {
            current.raw_data.clear();
            char* token = strtok(const_cast<char*>(val.c_str()), " \t");
            while (token) {
                int v = atoi(token);
                if (v > 0)
                    current.raw_data.push_back((uint16_t)v);
                token = strtok(nullptr, " \t");
            }
        }
    }
    if (in_code && !current.name.empty())
        codes.push_back(current);

    fclose(f);
    ESP_LOGI(TAG, "%s: %d komut okundu.", filepath.c_str(), (int)codes.size());
    return codes;
}

bool IrService::ReplayFile(const std::string& filepath, const std::string& command_name) {
    if (!initialized_)
        return false;

    auto codes = ParseIrFile(filepath);
    if (codes.empty()) {
        ESP_LOGE(TAG, "No IR code found in file.");
        return false;
    }

    const IrCode* target = nullptr;
    if (command_name.empty()) {
        target = &codes[0];
    } else {
        for (auto& c : codes) {
            if (c.name == command_name) {
                target = &c;
                break;
            }
        }
        if (!target) {
            ESP_LOGE(TAG, "Komut bulunamadı: %s", command_name.c_str());
            return false;
        }
    }

    ESP_LOGI(TAG, "Gönderiliyor: '%s' (%s) @ %luHz", target->name.c_str(), target->type.c_str(),
             (unsigned long)target->frequency);
    return SendCode(*target);
}

struct TvBGoneTaskArg {
    IrService* svc;
    std::string region;
};

static void TvBGoneTask(void* arg) {
    TvBGoneTaskArg* typed_arg = static_cast<TvBGoneTaskArg*>(arg);
    IrService* svc = typed_arg->svc;
    std::string region = typed_arg->region;
    delete typed_arg;

    const TvCode* arr = nullptr;
    size_t count = 0;

    if (region == "eu") {
        arr = TvBGoneCodes::TV_B_GONE_EU_CODES;
        count = TvBGoneCodes::TV_B_GONE_EU_CODE_COUNT;
        ESP_LOGI("IrService", "TV-B-Gone (EU) başladı. %d kod gönderilecek.", (int)count);
    } else {
        arr = TvBGoneCodes::TV_B_GONE_US_CODES;
        count = TvBGoneCodes::TV_B_GONE_US_CODE_COUNT;
        ESP_LOGI("IrService", "TV-B-Gone (US/AS) başladı. %d kod gönderilecek.", (int)count);
    }

    while (svc->IsTvBGoneRunning()) {
        for (size_t i = 0; i < count && svc->IsTvBGoneRunning(); i++) {
            std::vector<uint16_t> raw;
            for (size_t j = 0; j < arr[i].len; j++) {
                raw.push_back(arr[i].data[j]);
            }
            svc->_SendRawPublic(raw, arr[i].freq);
            vTaskDelay(pdMS_TO_TICKS(200));
        }
        vTaskDelay(pdMS_TO_TICKS(500));
    }

    svc->_SetTvBGone(false);
    ESP_LOGI("IrService", "TV-B-Gone tamamlandı.");
    vTaskDelete(NULL);
}

bool IrService::StartTvBGone(const std::string& region) {
    if (!initialized_)
        return false;
    if (is_tvbgone_)
        return true;
    is_tvbgone_ = true;

    TvBGoneTaskArg* arg = new TvBGoneTaskArg{this, region};
    xTaskCreatePinnedToCore(TvBGoneTask, "tvbgone", 4096, arg, 4,
                            (TaskHandle_t*)&tvbgone_task_handle_, 1);
    return true;
}

void IrService::StopTvBGone() { is_tvbgone_ = false; }

struct JammerTaskArg {
    IrService* svc;
    IrJamMode mode;
    uint32_t duration_ms;
};

static void IrJammerTask(void* arg) {
    JammerTaskArg* a = static_cast<JammerTaskArg*>(arg);
    IrService* svc = a->svc;
    IrJamMode mode = a->mode;
    uint32_t duration_ms = a->duration_ms;
    free(a);

    const uint64_t start = esp_timer_get_time() / 1000;
    ESP_LOGI("IrService", "IR Jammer başladı (mod: %d)", (int)mode);

    uint32_t sweep_timing = 8;
    int8_t sweep_dir = 1;

    while (svc->IsJamming()) {
        if (duration_ms > 0) {
            uint64_t elapsed = esp_timer_get_time() / 1000 - start;
            if (elapsed >= duration_ms)
                break;
        }

        switch (mode) {
            case IrJamMode::BASIC: {
                std::vector<uint16_t> raw;
                for (int i = 0; i < 40; i++) {
                    raw.push_back(12);
                    raw.push_back(12);
                }
                svc->_SendRawPublic(raw, 38000);
                break;
            }
            case IrJamMode::SWEEP: {
                sweep_timing += sweep_dir;
                if (sweep_timing >= 70 || sweep_timing <= 8)
                    sweep_dir = -sweep_dir;
                uint32_t freq = 33000 + (sweep_timing - 8) * (56000 - 33000) / 62;
                std::vector<uint16_t> raw;
                for (int i = 0; i < 30; i++) {
                    raw.push_back(sweep_timing);
                    raw.push_back(sweep_timing);
                }
                svc->_SendRawPublic(raw, freq);
                break;
            }
            case IrJamMode::RANDOM: {
                uint32_t freq = 36000 + (esp_timer_get_time() % 8000);
                std::vector<uint16_t> raw;
                for (int i = 0; i < 20; i++) {
                    raw.push_back((uint16_t)(5 + esp_timer_get_time() % 80));
                    raw.push_back((uint16_t)(5 + esp_timer_get_time() % 80));
                }
                svc->_SendRawPublic(raw, freq);
                break;
            }
        }
        vTaskDelay(pdMS_TO_TICKS(1));
    }

    svc->_SetJamming(false);
    ESP_LOGI("IrService", "IR Jammer stopped.");
    vTaskDelete(NULL);
}

bool IrService::StartJammer(IrJamMode mode, uint32_t duration_ms) {
    if (!initialized_)
        return false;
    if (is_jamming_)
        return true;
    is_jamming_ = true;

    JammerTaskArg* arg = (JammerTaskArg*)malloc(sizeof(JammerTaskArg));
    arg->svc = this;
    arg->mode = mode;
    arg->duration_ms = duration_ms;

    xTaskCreatePinnedToCore(IrJammerTask, "ir_jammer", 4096, arg, 5,
                            (TaskHandle_t*)&jammer_task_handle_, 1);
    return true;
}

void IrService::StopJammer() { is_jamming_ = false; }
