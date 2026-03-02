#include "cc1101_service.h"
#include <esp_log.h>
#include <esp_timer.h>
#include <dirent.h>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <iomanip>
#include <sstream>
#include "boards/t-embed/config.h"

#define TAG "Cc1101Service"

#define CC1101_SRES 0x30
#define CC1101_SFSTX 0x31
#define CC1101_SXOFF 0x32
#define CC1101_SCAL 0x33
#define CC1101_SRX 0x34
#define CC1101_STX 0x35
#define CC1101_SIDLE 0x36
#define CC1101_SFRX 0x3A
#define CC1101_SFTX 0x3B

#define CC1101_FREQ2 0x0D
#define CC1101_FREQ1 0x0E
#define CC1101_FREQ0 0x0F
#define CC1101_MDMCFG2 0x12
#define CC1101_RSSI 0x34

#define MAX_CAPTURE_SAMPLES 4096

Cc1101Service::Cc1101Service() {}

Cc1101Service::~Cc1101Service() {
    if (spi_handle_) {
        spi_bus_remove_device(spi_handle_);
    }
}

bool Cc1101Service::Init() {
    if (initialized_)
        return true;

    spi_device_interface_config_t devcfg = {};
    devcfg.clock_speed_hz = 10 * 1000 * 1000;
    devcfg.mode = 0;
    devcfg.spics_io_num = LORA_CS_PIN;
    devcfg.queue_size = 7;

    esp_err_t ret = spi_bus_add_device(SPI2_HOST, &devcfg, &spi_handle_);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to add SPI device: %s", esp_err_to_name(ret));
        return false;
    }

    Reset();
    SetRfSettings();

    initialized_ = true;
    ESP_LOGI(TAG, "CC1101 initialized successfully");
    return true;
}

void Cc1101Service::Reset() {
    CommandStrobe(CC1101_SRES);
    vTaskDelay(pdMS_TO_TICKS(10));
}

void Cc1101Service::CommandStrobe(uint8_t cmd) {
    spi_transaction_t t = {};
    t.length = 8;
    t.tx_buffer = &cmd;
    spi_device_transmit(spi_handle_, &t);
}

void Cc1101Service::_CommandStrobe(uint8_t cmd) { CommandStrobe(cmd); }

void Cc1101Service::WriteReg(uint8_t addr, uint8_t data) {
    uint8_t buf[2] = {addr, data};
    spi_transaction_t t = {};
    t.length = 16;
    t.tx_buffer = buf;
    spi_device_transmit(spi_handle_, &t);
}

uint8_t Cc1101Service::ReadReg(uint8_t addr) {
    uint8_t tx_buf[2] = {(uint8_t)(addr | 0x80), 0};
    uint8_t rx_buf[2] = {0, 0};
    spi_transaction_t t = {};
    t.length = 16;
    t.tx_buffer = tx_buf;
    t.rx_buffer = rx_buf;
    spi_device_transmit(spi_handle_, &t);
    return rx_buf[1];
}

void Cc1101Service::SetRfSettings() { SetFrequency(433.92f); }

bool Cc1101Service::SetFrequency(float mhz) {
    if (!initialized_)
        return false;
    uint32_t freq = (uint32_t)((mhz * 65536.0) / 26.0);
    WriteReg(CC1101_FREQ2, (freq >> 16) & 0xFF);
    WriteReg(CC1101_FREQ1, (freq >> 8) & 0xFF);
    WriteReg(CC1101_FREQ0, freq & 0xFF);
    current_frequency_mhz_ = mhz;
    ESP_LOGI(TAG, "Frequency set to %.2f MHz", mhz);
    return true;
}

bool Cc1101Service::SetModulation(const std::string& type) {
    if (!initialized_)
        return false;
    uint8_t mdmcfg2 = ReadReg(CC1101_MDMCFG2) & 0x8F;
    if (type == "ASK" || type == "OOK") {
        mdmcfg2 |= 0x30;
    } else if (type == "FSK") {
        mdmcfg2 |= 0x00;
    } else if (type == "GFSK") {
        mdmcfg2 |= 0x10;
    } else {
        return false;
    }
    WriteReg(CC1101_MDMCFG2, mdmcfg2);
    current_modulation_ = type;
    ESP_LOGI(TAG, "Modulation set to %s", type.c_str());
    return true;
}

bool Cc1101Service::RxStart(uint32_t timeout_ms) {
    if (!initialized_)
        return false;
    ClearCapture();
    rx_start_time_ms_ = esp_timer_get_time() / 1000;
    rx_active_ = true;
    CommandStrobe(CC1101_SRX);
    ESP_LOGI(TAG, "RX started for %lu ms", timeout_ms);
    return true;
}

bool Cc1101Service::RxStop() {
    if (!initialized_)
        return false;
    CommandStrobe(CC1101_SIDLE);
    rx_active_ = false;
    ESP_LOGI(TAG, "RX stopped, %zu samples captured", capture_buffer_.size());
    return true;
}

float Cc1101Service::ReadRssi() {
    if (!initialized_)
        return -100.0f;
    uint8_t rssi_raw = ReadReg(CC1101_RSSI | 0xC0);
    float rssi_dbm;
    if (rssi_raw >= 128) {
        rssi_dbm = ((float)rssi_raw - 256) / 2.0f - 74.0f;
    } else {
        rssi_dbm = (float)rssi_raw / 2.0f - 74.0f;
    }

    if (rx_active_ && capture_buffer_.size() < MAX_CAPTURE_SAMPLES) {
        CapturedSample s;
        s.timestamp_ms = (esp_timer_get_time() / 1000) - rx_start_time_ms_;
        s.rssi_dbm = rssi_dbm;
        capture_buffer_.push_back(s);
    }

    return rssi_dbm;
}

bool Cc1101Service::GetStatus() { return initialized_; }

void Cc1101Service::ClearCapture() {
    capture_buffer_.clear();
    rx_start_time_ms_ = 0;
}

void Cc1101Service::RecordSample() { ReadRssi(); }

size_t Cc1101Service::GetCaptureCount() const { return capture_buffer_.size(); }

std::string Cc1101Service::GetCaptureSummary() const {
    if (capture_buffer_.empty()) {
        return "No capture data available. Start RX and read RSSI to record samples.";
    }

    float min_rssi = capture_buffer_[0].rssi_dbm;
    float max_rssi = capture_buffer_[0].rssi_dbm;
    float avg_rssi = 0.0f;
    int64_t duration_ms = capture_buffer_.back().timestamp_ms;

    for (const auto& s : capture_buffer_) {
        if (s.rssi_dbm < min_rssi)
            min_rssi = s.rssi_dbm;
        if (s.rssi_dbm > max_rssi)
            max_rssi = s.rssi_dbm;
        avg_rssi += s.rssi_dbm;
    }
    avg_rssi /= (float)capture_buffer_.size();

    std::ostringstream oss;
    oss << std::fixed << std::setprecision(1);
    oss << "Captured " << capture_buffer_.size() << " RSSI samples over " << duration_ms << " ms. "
        << "Min: " << min_rssi << " dBm, "
        << "Max: " << max_rssi << " dBm, "
        << "Avg: " << avg_rssi << " dBm. "
        << "Frequency: " << current_frequency_mhz_ << " MHz, "
        << "Modulation: " << current_modulation_ << ".";

    if (max_rssi > -70.0f) {
        oss << " Strong signal detected (above -70 dBm).";
    } else if (max_rssi > -85.0f) {
        oss << " Moderate signal detected.";
    } else {
        oss << " No significant signal detected (all readings below -85 dBm).";
    }

    return oss.str();
}

bool Cc1101Service::SaveCaptureToSd(const std::string& filename) {
    if (capture_buffer_.empty()) {
        ESP_LOGW(TAG, "SaveCaptureToSd: buffer is empty");
        return false;
    }

    std::string path = std::string(SDCARD_MOUNT_POINT) + "/" + filename;
    if (path.find(".csv") == std::string::npos) {
        path += ".csv";
    }

    FILE* f = fopen(path.c_str(), "w");
    if (!f) {
        ESP_LOGE(TAG, "Failed to open file for writing: %s", path.c_str());
        return false;
    }

    fprintf(f, "timestamp_ms,rssi_dbm,frequency_mhz,modulation\n");
    for (const auto& s : capture_buffer_) {
        fprintf(f, "%lld,%.1f,%.2f,%s\n", (long long)s.timestamp_ms, s.rssi_dbm,
                current_frequency_mhz_, current_modulation_.c_str());
    }
    fclose(f);

    ESP_LOGI(TAG, "Saved %zu samples to %s", capture_buffer_.size(), path.c_str());
    return true;
}

 
 
 

static void JammerTask(void* arg) {
    Cc1101Service* service = static_cast<Cc1101Service*>(arg);
    ESP_LOGI(TAG, "Jammer task started on GPIO %d", LORA_GDO0_PIN);

    gpio_set_direction(LORA_GDO0_PIN, GPIO_MODE_OUTPUT);

     
    const uint32_t MAX_SEQUENCE = 50;
    const uint32_t DURATION_CYCLES = 3;
    uint32_t sequenceValues[MAX_SEQUENCE];
    for (int i = 0; i < MAX_SEQUENCE; i++) {
        sequenceValues[i] = 10 * (i + 1);
    }

    while (service->IsJamming()) {
        for (int sequence = 0; sequence < MAX_SEQUENCE && service->IsJamming(); sequence++) {
            uint32_t pulseWidth = sequenceValues[sequence];

            for (int duration = 0; duration < DURATION_CYCLES && service->IsJamming(); duration++) {
                 
                gpio_set_level(LORA_GDO0_PIN, 1);
                for (uint32_t i = 0; i < pulseWidth; i += 10) {
                    gpio_set_level(LORA_GDO0_PIN, 1);
                    esp_rom_delay_us(5);
                    if (i % 20 == 0) {
                        gpio_set_level(LORA_GDO0_PIN, 0);
                        esp_rom_delay_us(2);
                        gpio_set_level(LORA_GDO0_PIN, 1);
                    }
                    esp_rom_delay_us(5);
                }
                gpio_set_level(LORA_GDO0_PIN, 0);
                uint32_t lowPeriod = pulseWidth + (pulseWidth % 23);
                for (uint32_t i = 0; i < lowPeriod; i += 10) {
                    gpio_set_level(LORA_GDO0_PIN, 0);
                    esp_rom_delay_us(10);
                }
                vTaskDelay(pdMS_TO_TICKS(1));   
            }
        }

         
        if (service->IsJamming()) {
            for (int i = 0; i < 100 && service->IsJamming(); i++) {
                uint32_t pulseWidth = 5 + (esp_timer_get_time() % 46);
                gpio_set_level(LORA_GDO0_PIN, 1);
                esp_rom_delay_us(pulseWidth);
                gpio_set_level(LORA_GDO0_PIN, 0);
                uint32_t spaceWidth = 5 + (esp_timer_get_time() % 96);
                esp_rom_delay_us(spaceWidth);
            }
            vTaskDelay(pdMS_TO_TICKS(1));
        }
    }

    gpio_set_level(LORA_GDO0_PIN, 0);
    ESP_LOGI(TAG, "Jammer task terminating");
    vTaskDelete(NULL);
}

static void JammerTimeoutCallback(void* arg) {
    Cc1101Service* service = static_cast<Cc1101Service*>(arg);
    if (service->IsJamming()) {
        ESP_LOGI(TAG, "Jammer timeout reached, stopping.");
        service->StopJammer();
    }
}

bool Cc1101Service::StartJammer(uint32_t duration_ms) {
    if (!initialized_)
        return false;
    if (is_jamming_)
        return true;   

     
    uint8_t iocfg0 = ReadReg(0x02);   
    WriteReg(0x02, 0x0D);             

     
    CommandStrobe(CC1101_STX);

    is_jamming_ = true;

     
    xTaskCreatePinnedToCore(JammerTask, "jammer_task", 4096, this, 5,
                            (TaskHandle_t*)&jammer_task_handle_, 1);

     
    if (duration_ms > 0) {
        esp_timer_create_args_t timer_args = {};
        timer_args.callback = &JammerTimeoutCallback;
        timer_args.arg = this;
        timer_args.name = "jammer_timeout";

        esp_timer_handle_t timer;
        esp_timer_create(&timer_args, &timer);
        esp_timer_start_once(timer, duration_ms * 1000);
    }

    ESP_LOGI(TAG, "Jammer started for %lu ms", duration_ms);
    return true;
}

bool Cc1101Service::StopJammer() {
    if (!is_jamming_)
        return false;

    is_jamming_ = false;   

     
    CommandStrobe(CC1101_SIDLE);
    gpio_set_level(LORA_GDO0_PIN, 0);

    jammer_task_handle_ = nullptr;
    ESP_LOGI(TAG, "Jammer stopped");
    return true;
}

 
 
 

void Cc1101Service::_PushRawSample(int32_t sample) {
    if (raw_capture_buffer_.size() < 20000) {
        raw_capture_buffer_.push_back(sample);
    }
}

static void RawCaptureTask(void* arg) {
    Cc1101Service* service = static_cast<Cc1101Service*>(arg);
    ESP_LOGI(TAG, "Raw capture task started on GPIO %d", LORA_GDO0_PIN);

    gpio_set_direction(LORA_GDO0_PIN, GPIO_MODE_INPUT);

    int last_level = gpio_get_level(LORA_GDO0_PIN);
    int64_t last_transition_time = esp_timer_get_time();

    while (service->IsRawCapturing()) {
        int current_level = gpio_get_level(LORA_GDO0_PIN);
        if (current_level != last_level) {
            int64_t current_time = esp_timer_get_time();
            int32_t duration_us = (int32_t)(current_time - last_transition_time);

             
            if (duration_us > 50) {
                 
                if (last_level == 1) {
                    service->_PushRawSample(duration_us);
                } else {
                    service->_PushRawSample(-duration_us);
                }
            }

            last_transition_time = current_time;
            last_level = current_level;
        }
        esp_rom_delay_us(10);
    }

    ESP_LOGI(TAG, "Raw capture task terminating");
    vTaskDelete(NULL);
}

static void RawCaptureTimeoutCallback(void* arg) {
    Cc1101Service* service = static_cast<Cc1101Service*>(arg);
    if (service->IsRawCapturing()) {
        ESP_LOGI(TAG, "Raw capture timeout reached, stopping.");
        service->StopRawCapture();
    }
}

bool Cc1101Service::StartRawCapture(uint32_t timeout_ms) {
    if (!initialized_)
        return false;
    if (is_raw_capturing_ || is_replaying_ || is_jamming_)
        return false;

    raw_capture_buffer_.clear();

     
     
    if (current_modulation_ != "ASK" && current_modulation_ != "OOK") {
        ESP_LOGI(TAG, "Temporarily switching to ASK/OOK for RAW capture");
        SetModulation("ASK");   
    }

     
     
    WriteReg(CC1101_MDMCFG4, 0x01);

     
     
    WriteReg(0x02, 0x0D);
     
    CommandStrobe(CC1101_SRX);

    is_raw_capturing_ = true;
    xTaskCreatePinnedToCore(RawCaptureTask, "raw_rx_task", 4096, this, 5,
                            (TaskHandle_t*)&raw_capture_task_handle_, 1);

    if (timeout_ms > 0) {
        esp_timer_create_args_t timer_args = {};
        timer_args.callback = &RawCaptureTimeoutCallback;
        timer_args.arg = this;
        timer_args.name = "raw_rx_timeout";

        esp_timer_handle_t timer;
        esp_timer_create(&timer_args, &timer);
        esp_timer_start_once(timer, timeout_ms * 1000);
    }

    ESP_LOGI(TAG, "Raw capture started");
    return true;
}

bool Cc1101Service::StopRawCapture() {
    if (!is_raw_capturing_)
        return false;
    is_raw_capturing_ = false;   

    CommandStrobe(CC1101_SIDLE);

     
     
    WriteReg(CC1101_MDMCFG4, 0xCA);

    raw_capture_task_handle_ = nullptr;
    ESP_LOGI(TAG, "Raw capture stopped. Samples: %zu", raw_capture_buffer_.size());
    return true;
}

bool Cc1101Service::SaveSubFile(const std::string& filename) const {
    if (raw_capture_buffer_.empty()) {
        ESP_LOGW(TAG, "SaveSubFile: buffer is empty");
        return false;
    }

    std::string path = std::string(SDCARD_MOUNT_POINT) + "/" + filename;
    if (path.find(".sub") == std::string::npos) {
        path += ".sub";
    }

    FILE* f = fopen(path.c_str(), "w");
    if (!f) {
        ESP_LOGE(TAG, "SaveSubFile: Failed to open %s", path.c_str());
        return false;
    }

    fprintf(f, "Filetype: Flipper SubGhz RAW File\n");
    fprintf(f, "Version: 1\n");
    fprintf(f, "Frequency: %lu\n", (uint32_t)(current_frequency_mhz_ * 1000000));
    fprintf(f, "Preset: FuriHalSubGhzPresetOok650Async\n");
    fprintf(f, "Protocol: RAW\n");

    fprintf(f, "RAW_Data: ");
    for (size_t i = 0; i < raw_capture_buffer_.size(); i++) {
        fprintf(f, "%ld ", (long)raw_capture_buffer_[i]);
        if ((i + 1) % 512 == 0 && i != raw_capture_buffer_.size() - 1) {
            fprintf(f, "\nRAW_Data: ");
        }
    }
    fprintf(f, "\n");
    fclose(f);

    ESP_LOGI(TAG, "Saved %zu RAW samples to %s", raw_capture_buffer_.size(), path.c_str());
    return true;
}

static void ReplayTask(void* arg) {
    Cc1101Service* service = static_cast<Cc1101Service*>(arg);
    ESP_LOGI(TAG, "Replay task started");

    gpio_set_direction(LORA_GDO0_PIN, GPIO_MODE_OUTPUT);

    service->_CommandStrobe(CC1101_STX);

    const auto& buffer = service->_GetRawBuffer();
    for (int32_t val : buffer) {
        if (!service->IsReplaying())
            break;

        if (val > 0) {
            gpio_set_level(LORA_GDO0_PIN, 1);
            esp_rom_delay_us(val);
        } else {
            gpio_set_level(LORA_GDO0_PIN, 0);
            esp_rom_delay_us(-val);
        }
    }

    gpio_set_level(LORA_GDO0_PIN, 0);
    service->_CommandStrobe(CC1101_SIDLE);

    service->_SetReplaying(false);
    ESP_LOGI(TAG, "Replay task terminating");
    vTaskDelete(NULL);
}

void Cc1101Service::_SetReplaying(bool state) { is_replaying_ = state; }

bool Cc1101Service::ReplaySubFile(const std::string& filename) {
    if (!initialized_)
        return false;
    if (is_replaying_ || is_jamming_ || is_raw_capturing_) {
        ESP_LOGE(TAG, "CC1101 is busy");
        return false;
    }

    std::string path = std::string(SDCARD_MOUNT_POINT) + "/" + filename;
    if (path.find(".sub") == std::string::npos) {
        path += ".sub";
    }

    FILE* f = fopen(path.c_str(), "r");
    if (!f) {
        ESP_LOGE(TAG, "ReplaySubFile: Failed to open %s", path.c_str());
        return false;
    }

    raw_capture_buffer_.clear();
    char line[1024];

    while (fgets(line, sizeof(line), f)) {
        std::string str_line(line);
         
        str_line.erase(std::remove(str_line.begin(), str_line.end(), '\r'), str_line.end());
        str_line.erase(std::remove(str_line.begin(), str_line.end(), '\n'), str_line.end());

        if (str_line.find("RAW_Data:") != std::string::npos) {
            char* token = strtok(line + 9, " \t\r\n");   
            while (token != NULL) {
                int32_t val = atol(token);
                if (val != 0) {
                    raw_capture_buffer_.push_back(val);
                }
                token = strtok(NULL, " \t\r\n");
            }
        } else if (str_line.find("Frequency:") != std::string::npos) {
            uint32_t freq_hz = 0;
            if (sscanf(str_line.c_str(), "Frequency: %lu", &freq_hz) == 1) {
                float freq_mhz = (float)freq_hz / 1000000.0f;
                SetFrequency(freq_mhz);
            }
        }
    }
    fclose(f);

    if (raw_capture_buffer_.empty()) {
        ESP_LOGE(TAG, "ReplaySubFile: No RAW data found in file");
        return false;
    }

     
    if (current_modulation_ != "ASK" && current_modulation_ != "OOK") {
        ESP_LOGI(TAG, "Temporarily switching to ASK/OOK for Replay Attack");
        SetModulation("ASK");   
    }
    WriteReg(CC1101_MDMCFG4, 0x01);   

    WriteReg(0x02, 0x0D);
    is_replaying_ = true;
    current_replay_file_ = filename;

    xTaskCreatePinnedToCore(ReplayTask, "replay_task", 4096, this, 5,
                            (TaskHandle_t*)&replay_task_handle_, 1);

    ESP_LOGI(TAG, "Replay started for %s (%zu samples)", path.c_str(), raw_capture_buffer_.size());
    return true;
}

std::string Cc1101Service::ListSubFiles() const {
    DIR* dir = opendir(SDCARD_MOUNT_POINT);
    if (!dir) {
        return "Error: Could not open " SDCARD_MOUNT_POINT ". Is SD card mounted?";
    }

    std::string result = "Files in " SDCARD_MOUNT_POINT ":\n";
    struct dirent* entry;
    int count = 0;
    while ((entry = readdir(dir)) != NULL) {
        std::string name = entry->d_name;
        if (name.find(".sub") != std::string::npos || name.find(".csv") != std::string::npos) {
            result += "- " + name + "\n";
            count++;
        }
    }
    closedir(dir);

    if (count == 0) {
        return "No .sub or .csv files found on SD card.";
    }
    return result;
}
