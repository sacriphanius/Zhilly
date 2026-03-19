#include "radio_service.h"
#include <sdkconfig.h>
#include <esp_log.h>
#include <esp_timer.h>
#include <esp_rom_sys.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <cstring>
#include <cmath>

#define TAG "RadioService"

// SX1262 Opcodes
#define SX126X_CMD_SET_STANDBY              0x80
#define SX126X_CMD_SET_TX_PARAMS            0x8E
#define SX126X_CMD_SET_RF_FREQUENCY         0x86
#define SX126X_CMD_SET_PA_CONFIG            0x95
#define SX126X_CMD_SET_TX_CONTINUOUS_WAVE   0xD1
#define SX126X_CMD_SET_REGULATOR_MODE       0x96
#define SX126X_CMD_SET_DIO_IRQ_PARAMS       0x08
#define SX126X_CMD_GET_STATUS               0xC0

RadioService& RadioService::GetInstance() {
    static RadioService instance;
    return instance;
}

RadioService::RadioService() {}

RadioService::~RadioService() {
    if (spi_handle_) {
        spi_bus_remove_device(spi_handle_);
    }
}

void RadioService::WaitBusy() {
    int timeout = 1000;
    while (gpio_get_level(RADIO_BUSY_PIN) == 1 && timeout-- > 0) {
        esp_rom_delay_us(100);
    }
}

void RadioService::Reset() {
    gpio_set_level(RADIO_RST_PIN, 0);
    vTaskDelay(pdMS_TO_TICKS(10));
    gpio_set_level(RADIO_RST_PIN, 1);
    vTaskDelay(pdMS_TO_TICKS(10));
    WaitBusy();
}

void RadioService::WriteCommand(uint8_t opcode, const uint8_t* data, size_t len) {
    std::lock_guard<std::mutex> lock(spi_mutex_);
    WaitBusy();
    gpio_set_level(RADIO_SPI_CS_PIN, 0);
    
    spi_transaction_t t = {};
    t.length = 8;
    t.flags = SPI_TRANS_USE_TXDATA;
    t.tx_data[0] = opcode;
    spi_device_transmit(spi_handle_, &t);
    
    if (len > 0) {
        t.length = len * 8;
        t.tx_buffer = data;
        t.flags = 0;
        spi_device_transmit(spi_handle_, &t);
    }
    
    gpio_set_level(RADIO_SPI_CS_PIN, 1);
}

void RadioService::ReadCommand(uint8_t opcode, uint8_t* data, size_t len) {
    std::lock_guard<std::mutex> lock(spi_mutex_);
    WaitBusy();
    gpio_set_level(RADIO_SPI_CS_PIN, 0);
    
    spi_transaction_t t = {};
    t.length = 8;
    t.flags = SPI_TRANS_USE_TXDATA;
    t.tx_data[0] = opcode;
    spi_device_transmit(spi_handle_, &t);
    
    // SX1262 requires a dummy byte after opcode for reads
    t.length = 8;
    t.tx_data[0] = 0x00;
    t.flags = SPI_TRANS_USE_TXDATA;
    spi_device_transmit(spi_handle_, &t);

    if (len > 0) {
        t.length = len * 8;
        t.rx_buffer = data;
        t.tx_buffer = nullptr;
        t.flags = 0;
        spi_device_transmit(spi_handle_, &t);
    }
    
    gpio_set_level(RADIO_SPI_CS_PIN, 1);
}

bool RadioService::Initialize() {
    if (initialized_) return true;

    ESP_LOGI(TAG, "Initializing SX1262 Radio...");

    gpio_set_direction(RADIO_RST_PIN, GPIO_MODE_OUTPUT);
    gpio_set_direction(RADIO_BUSY_PIN, GPIO_MODE_INPUT);
    gpio_set_direction(RADIO_SPI_CS_PIN, GPIO_MODE_OUTPUT);
    gpio_set_level(RADIO_SPI_CS_PIN, 1);

    spi_bus_config_t buscfg = {};
    buscfg.mosi_io_num = RADIO_SPI_MOSI_PIN;
    buscfg.miso_io_num = RADIO_SPI_MISO_PIN;
    buscfg.sclk_io_num = RADIO_SPI_SCK_PIN;
    buscfg.quadwp_io_num = -1;
    buscfg.quadhd_io_num = -1;
    
    esp_err_t ret = spi_bus_initialize(SPI2_HOST, &buscfg, SPI_DMA_CH_AUTO);
    if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) {
        ESP_LOGE(TAG, "Failed to initialize SPI bus");
        return false;
    }

    spi_device_interface_config_t devcfg = {};
    devcfg.clock_speed_hz = 8 * 1000 * 1000;
    devcfg.mode = 0;
    devcfg.spics_io_num = -1; // Manual CS
    devcfg.queue_size = 7;

    ret = spi_bus_add_device(SPI2_HOST, &devcfg, &spi_handle_);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to add SPI device");
        return false;
    }

    Reset();

    // Basic startup sequence
    SetStandby(false); // STDBY_RC
    
    // Set DIO3 as TCXO ctrl (1.8V, 5ms timeout) - Typical for LilyGO S3
    uint8_t tcxoBuf[4] = {0x01, 0x00, 0x00, 0x64}; // 1.8V, 5ms
    WriteCommand(0x97, tcxoBuf, 4);
    
    // Set DIO2 as RF Switch Ctrl
    uint8_t dio2Mode = 0x01;
    WriteCommand(0x9D, &dio2Mode, 1);

    uint8_t regMode = 0x01; // DC-DC
    WriteCommand(SX126X_CMD_SET_REGULATOR_MODE, &regMode, 1);
    
    SetPaConfig();
    
    uint8_t status;
    ReadCommand(SX126X_CMD_GET_STATUS, &status, 1);
    ESP_LOGI(TAG, "SX1262 Status: 0x%02X", status);

    initialized_ = true;
    return true;
}

void RadioService::SetStandby(bool xosc) {
    uint8_t mode = xosc ? 0x01 : 0x00; // STDBY_XOSC or STDBY_RC
    WriteCommand(SX126X_CMD_SET_STANDBY, &mode, 1);
}

void RadioService::SetPaConfig() {
    uint8_t buf[4];
    buf[0] = 0x04; // dutyCycle
    buf[1] = 0x07; // hpMax
    buf[2] = 0x00; // deviceSel (SX1262)
    buf[3] = 0x01; // paLut
    WriteCommand(SX126X_CMD_SET_PA_CONFIG, buf, 4);
}

void RadioService::SetTxParams(int8_t power) {
    uint8_t buf[2];
    buf[0] = power;
    buf[1] = 0x02; // rampTime (40us)
    WriteCommand(SX126X_CMD_SET_TX_PARAMS, buf, 2);
}

void RadioService::SetRfFrequency(uint32_t frequency) {
    uint8_t buf[4];
    uint32_t freq = (uint32_t)((double)frequency * (double)(1 << 25) / 32000000.0);
    buf[0] = (freq >> 24) & 0xFF;
    buf[1] = (freq >> 16) & 0xFF;
    buf[2] = (freq >> 8) & 0xFF;
    buf[3] = freq & 0xFF;
    WriteCommand(SX126X_CMD_SET_RF_FREQUENCY, buf, 4);
}

void RadioService::SetTxContinuousWave() {
    WriteCommand(SX126X_CMD_SET_TX_CONTINUOUS_WAVE, nullptr, 0);
}

bool RadioService::TransmitTeslaPortSignal() {
    if (!initialized_ && !Initialize()) return false;

    ESP_LOGI(TAG, "Transmitting Tesla Port Open signal (433.92 MHz Toggled CW)...");

    SetStandby(true); // Ensure XOSC is running
    SetRfFrequency(433920000);
    SetPaConfig();
    SetTxParams(22); // Max power
    
    // Tesla AM650 Raw timings
    static const uint16_t TESLA_RAW[] = {
        400, 400, 400, 400, 400, 400, 400, 400, 400, 400, 400, 400, 400, 400, 400, 400, 400, 400, 400, 400, 
        400, 400, 400, 400, 400, 1200, 400, 400, 400, 400, 800, 800, 400, 400, 800, 800, 800, 800, 400, 400, 
        800, 800, 800, 800, 800, 800, 800, 800, 800, 800, 400, 400, 800, 400, 400, 800, 800, 400, 400, 800, 
        400, 400, 800, 400, 400, 400, 400, 800, 400, 400, 400, 400, 800, 400, 400, 800, 800, 400, 400, 800, 
        800, 800, 400, 400, 400, 400, 400, 400, 800, 400, 400, 800, 400, 400, 800, 1200
    };

    for (int rep = 0; rep < 5; rep++) {
        for (size_t i = 0; i < sizeof(TESLA_RAW)/sizeof(uint16_t); i++) {
            if (i % 2 == 0) {
                SetTxContinuousWave();
            } else {
                SetStandby(true);
            }
            esp_rom_delay_us(TESLA_RAW[i]);
        }
        SetStandby(true);
        vTaskDelay(pdMS_TO_TICKS(50));
    }

    SetStandby(false); // back to RC to save power
    ESP_LOGI(TAG, "Transmission finished.");
    return true;
}

// Global for task (simple approach for now)
static uint32_t jam_center_freq = 433920000;

// ---------------------------------------------------------------------------
// RF Jammer Implementation
// ---------------------------------------------------------------------------
void RadioService::JammerTask(void* arg) {
    RadioService* svc = static_cast<RadioService*>(arg);
    // Re-initialize for clean state
    svc->Initialize();
    
    ESP_LOGI("RadioService", "Jammer task started around %lu Hz", jam_center_freq);
    
    svc->SetStandby(true);
    svc->SetPaConfig();
    svc->SetTxParams(22);

    uint32_t start_freq = jam_center_freq - 500000;
    uint32_t end_freq = jam_center_freq + 500000;
    uint32_t step = 20000;

    while (svc->IsJamming()) {
        for (uint32_t f = start_freq; f <= end_freq && svc->IsJamming(); f += step) {
            svc->SetRfFrequency(f);
            svc->SetTxContinuousWave();
            esp_rom_delay_us(200);
        }
    }

    svc->SetStandby(false);
    ESP_LOGI("RadioService", "Jammer task finished.");
    svc->jammer_task_handle_ = nullptr;
    vTaskDelete(NULL);
}

bool RadioService::StartJammer(uint32_t freq, uint32_t duration_ms) {
    if (is_jamming_) return true;
    if (!initialized_ && !Initialize()) return false;

    is_jamming_ = true;
    jam_center_freq = freq;

    xTaskCreatePinnedToCore(RadioService::JammerTask, "jammer_task", 4096, this, 5, &jammer_task_handle_, 1);

    if (duration_ms > 0) {
        esp_timer_create_args_t args = {};
        args.callback = [](void* arg) {
            static_cast<RadioService*>(arg)->StopJammer();
        };
        args.arg = this;
        args.name = "jammer_to";
        esp_timer_create(&args, &jammer_timer_);
        esp_timer_start_once(jammer_timer_, duration_ms * 1000ULL);
    }

    ESP_LOGI(TAG, "Jammer started on %lu Hz for %lu ms", freq, duration_ms);
    return true;
}

bool RadioService::StopJammer() {
    if (!is_jamming_) return false;
    is_jamming_ = false;
    if (jammer_timer_) {
        esp_timer_stop(jammer_timer_);
        esp_timer_delete(jammer_timer_);
        jammer_timer_ = nullptr;
    }
    ESP_LOGI(TAG, "Jammer stopped");
    return true;
}

std::string RadioService::GetInfo() {
    if (!initialized_) return "SX1262 not initialized";
    uint8_t status;
    ReadCommand(SX126X_CMD_GET_STATUS, &status, 1);
    char buf[64];
    snprintf(buf, sizeof(buf), "SX1262 Ready (Status: 0x%02X)", status);
    return std::string(buf);
}
